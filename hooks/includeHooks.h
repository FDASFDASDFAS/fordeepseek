#pragma once

#include "../../../external/kiero/minhook/include/MinHook.h"
#include <cstdint>

#define CS_CONCATENATE_DETAIL(x, y) x##y
#define CS_CONCATENATE(x, y) CS_CONCATENATE_DETAIL(x, y)

#define CS_CLASS_NO_CONSTRUCTOR(CLASS)	\
CLASS() = delete;						\
CLASS(CLASS&&) = delete;				\
CLASS(const CLASS&) = delete;


#define CS_CLASS_NO_ASSIGNMENT(CLASS)	\
CLASS& operator=(CLASS&&) = delete;		\
CLASS& operator=(const CLASS&) = delete;

#define CS_CLASS_NO_INITIALIZER(CLASS)	\
CS_CLASS_NO_CONSTRUCTOR(CLASS)			\
CS_CLASS_NO_ASSIGNMENT(CLASS)

#define MEM_PAD(SIZE) \
private: \
    char CS_CONCATENATE(pad_, __COUNTER__)[SIZE]; \
public:

// @source: popular github minhook instance
template <typename T>
class CInlineHookObj
{
public:
	CInlineHookObj() = default;
	CInlineHookObj(const CInlineHookObj&) = delete;
	CInlineHookObj& operator=(const CInlineHookObj&) = delete;
	CInlineHookObj(CInlineHookObj&& other) noexcept
	{
		MoveFrom(other);
	}
	CInlineHookObj& operator=(CInlineHookObj&& other) noexcept
	{
		if (this != &other && pBaseFn == nullptr)
			MoveFrom(other);
		return *this;
	}

	/// setup hook and replace function
	/// @returns: true if hook has been successfully created, false otherwise
	bool Add(void* pFunction, void* pDetour)
	{
		if (pFunction == nullptr || pDetour == nullptr || pBaseFn != nullptr)
			return false;

		pBaseFn = pFunction;
		pReplaceFn = pDetour;

		if (const MH_STATUS status = MH_CreateHook(pBaseFn, pReplaceFn, &pOriginalFn); status != MH_OK)
		{
			Reset();
			return false;
		}

		return true;
	}

	/// patch memory to jump to our function instead of original
	/// @returns: true if hook has been successfully applied, false otherwise
	bool Replace()
	{
		// check is hook has been created
		if (pBaseFn == nullptr)
			return false;

		// check that function isn't already hooked
		if (bIsHooked)
			return false;

		if (const MH_STATUS status = MH_EnableHook(pBaseFn); status != MH_OK)
		{
			return false;
		}

		// switch hook state
		bIsHooked = true;
		return true;
	}

	/// restore original function call and cleanup hook data
	/// @returns: true if hook has been successfully removed, false otherwise
	bool Remove()
	{
		if (pBaseFn == nullptr)
			return true;

		const MH_STATUS disableStatus = MH_DisableHook(pBaseFn);
		if (disableStatus != MH_OK && disableStatus != MH_ERROR_DISABLED && disableStatus != MH_ERROR_NOT_CREATED)
		{
			return false;
		}

		const MH_STATUS removeStatus = MH_RemoveHook(pBaseFn);
		if (removeStatus != MH_OK && removeStatus != MH_ERROR_NOT_CREATED)
			return false;

		Reset();
		return true;
	}

	/// restore patched memory to original function call
	/// @returns: true if hook has been successfully restored, false otherwise
	bool Restore()
	{
		// check that function is hooked
		if (!bIsHooked)
			return true;

		if (const MH_STATUS status = MH_DisableHook(pBaseFn); status != MH_OK && status != MH_ERROR_DISABLED)
		{
			return false;
		}

		// switch hook state
		bIsHooked = false;
		return true;
	}

	/// @returns: original, unwrapped function that would be called without the hook
	inline T GetOriginal()
	{
		return reinterpret_cast<T>(pOriginalFn);
	}

	/// @returns: true if hook is applied at the time, false otherwise
	inline bool IsHooked() const
	{
		return bIsHooked;
	}

private:
	void MoveFrom(CInlineHookObj& other) noexcept
	{
		bIsHooked = other.bIsHooked;
		pBaseFn = other.pBaseFn;
		pReplaceFn = other.pReplaceFn;
		pOriginalFn = other.pOriginalFn;
		other.Reset();
	}

	void Reset()
	{
		bIsHooked = false;
		pBaseFn = nullptr;
		pReplaceFn = nullptr;
		pOriginalFn = nullptr;
	}

	// current hook state
	bool bIsHooked = false;
	// function base handle
	void* pBaseFn = nullptr;
	// function that being replace the original call
	void* pReplaceFn = nullptr;
	// original function
	void* pOriginalFn = nullptr;
};
