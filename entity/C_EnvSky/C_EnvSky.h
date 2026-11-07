#pragma once

#include "../../../templeware/utils/schema/schema.h"
#include "../C_Material/C_Material.h"
#include <format>
#include <array>
#include "../../external/imgui/imgui.h"

enum
{
	COLOR_R = 0,
	COLOR_G = 1,
	COLOR_B = 2,
	COLOR_A = 3
};

struct ColorRGBExp32
{
	char r, g, b;
	signed char exponent;
};

class Color
{
public:
	Color() = default;

	/* default color constructor (in: 0 - 255) */
	constexpr Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) :
		arrColor({ r, g, b, a }) {
	}

	constexpr Color(int r, int g, int b, int a = 255) :
		arrColor({ static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), static_cast<std::uint8_t>(a) }) {
	}

	/* float color constructor (in: 0.0 - 1.0) */
	constexpr Color(float r, float g, float b, float a = 1.0f) :
		arrColor({ static_cast<std::uint8_t>(r * 255.f), static_cast<std::uint8_t>(g * 255.f), static_cast<std::uint8_t>(b * 255.f), static_cast<std::uint8_t>(a * 255.f) }) {
	}

	/* alpha modifier coonstructor */
	constexpr Color(const Color& colSecond, int a) :
		arrColor({ colSecond.arrColor.at(0), colSecond.arrColor.at(1), colSecond.arrColor.at(2), static_cast<std::uint8_t>(a) }) {
	}

	constexpr Color(const Color& colSecond, float a) :
		arrColor({ colSecond.arrColor.at(0), colSecond.arrColor.at(1), colSecond.arrColor.at(2), static_cast<std::uint8_t>(a * 255.f) }) {
	}

	/* output color to given variables */
	void Get(std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a) const
	{
		r = arrColor.at(COLOR_R);
		g = arrColor.at(COLOR_G);
		b = arrColor.at(COLOR_B);
		a = arrColor.at(COLOR_A);
	}

	void Set(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
	{
		arrColor.at(0) = r;
		arrColor.at(1) = g;
		arrColor.at(2) = b;
		arrColor.at(3) = a;
	}

	void Set(float r, float g, float b, float a)
	{
		arrColor.at(0) = (std::uint8_t)(r * 255.f);
		arrColor.at(1) = (std::uint8_t)(g * 255.f);
		arrColor.at(2) = (std::uint8_t)(b * 255.f);
		arrColor.at(3) = (std::uint8_t)(a * 255.f);
	}

	inline std::uint8_t r() const { return arrColor.at(0); }
	inline std::uint8_t g() const { return arrColor.at(1); }
	inline std::uint8_t b() const { return arrColor.at(2); }
	inline std::uint8_t a() const { return arrColor.at(3); }

	inline float rBase() const { return arrColor.at(0) / 255.f; }
	inline float gBase() const { return arrColor.at(1) / 255.f; }
	inline float bBase() const { return arrColor.at(2) / 255.f; }
	inline float aBase() const { return arrColor.at(3) / 255.f; }

	const std::array<std::uint8_t, 4>& GetArray() const { return arrColor; }

	/* convert color to directx argb */
	/*D3DCOLOR GetD3D( ) const
	{
		return D3DCOLOR_ARGB( arrColor.at( COLOR_A ), arrColor.at( COLOR_R ), arrColor.at( COLOR_G ), arrColor.at( COLOR_B ) );
	}*/

	/* convert color to imgui rgba */
	ImU32 GetU32(const float flAlphaMultiplier = 1.0f) const
	{
		return ImGui::GetColorU32(this->GetVec4(flAlphaMultiplier));
	}

	/* convert color to imgui vector */
	ImVec4 GetVec4(const float flAlphaMultiplier = 1.0f) const
	{
		return ImVec4(this->Base<COLOR_R>(), this->Base<COLOR_G>(), this->Base<COLOR_B>(), this->Base<COLOR_A>() * flAlphaMultiplier);
	}

	std::uint8_t& operator[](const std::size_t i)
	{
		return this->arrColor.data()[i];
	}

	const std::uint8_t& operator[](const std::size_t i) const
	{
		return this->arrColor.data()[i];
	}

	bool operator==(const Color& colSecond) const
	{
		return *const_cast<Color*>(this) == *const_cast<Color*>(&colSecond);
	}

	bool operator!=(const Color& colSecond) const
	{
		return !(operator==(colSecond));
	}

	Color& operator=(const Color& colFrom)
	{
		arrColor.at(0) = colFrom.arrColor.at(COLOR_R);
		arrColor.at(1) = colFrom.arrColor.at(COLOR_G);
		arrColor.at(2) = colFrom.arrColor.at(COLOR_B);
		arrColor.at(3) = colFrom.arrColor.at(COLOR_A);
		return *this;
	}

	Color Gradient() const
	{
		Color copy = *this;

		copy.arrColor.at(0) /= 3;
		copy.arrColor.at(1) /= 3;
		copy.arrColor.at(2) /= 3;

		return copy;
	}

	/* returns certain R/G/B/A value */
	template <std::size_t N>
	std::uint8_t Get() const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "given index is out of range");
		return arrColor[N];
	}

	/* returns copy of color with changed certain R/G/B/A value to given value */
	template <std::size_t N>
	Color Set(const std::uint8_t nValue) const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "given index is out of range");

		Color colCopy = *this;
		colCopy.arrColor[N] = nValue;
		return colCopy;
	}

	/* returns copy of color with multiplied certain R/G/B/A value by given value */
	template <std::size_t N>
	Color Multiplier(const float flValue) const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "given index is out of range");

		Color colCopy = *this;
		colCopy.arrColor[N] = static_cast<std::uint8_t>(static_cast<float>(colCopy.arrColor[N]) * flValue);
		return colCopy;
	}

	/* returns copy of color with divided certain R/G/B/A value by given value */
	template <std::size_t N>
	Color Divider(const int iValue) const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "given index is out of range");

		Color colCopy = *this;
		colCopy.arrColor[N] /= iValue;
		return colCopy;
	}

	/* returns certain R/G/B/A float value (in: 0 - 255, out: 0.0 - 1.0) */
	template <std::size_t N>
	float Base() const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "given index is out of range");
		return arrColor[N] / 255.f;
	}

	/* convert color to float array (in: 0 - 255, out: 0.0 - 1.0) */
	std::array<float, 3U> Base() const
	{
		std::array<float, 3U> arrBaseColor = { };
		arrBaseColor.at(COLOR_R) = this->Base<COLOR_R>();
		arrBaseColor.at(COLOR_G) = this->Base<COLOR_G>();
		arrBaseColor.at(COLOR_B) = this->Base<COLOR_B>();
		return arrBaseColor;
	}

	/* set color from float array (in: 0.0 - 1.0, out: 0 - 255) */
	static Color FromBase3(float arrBase[3])
	{
		return Color(arrBase[0], arrBase[1], arrBase[2]);
	}

	/* convert color to float array w/ alpha (in: 0 - 255, out: 0.0 - 1.0) */
	std::array<float, 4U> BaseAlpha() const
	{
		std::array<float, 4U> arrBaseColor = { };
		arrBaseColor.at(COLOR_R) = this->Base<COLOR_R>();
		arrBaseColor.at(COLOR_G) = this->Base<COLOR_G>();
		arrBaseColor.at(COLOR_B) = this->Base<COLOR_B>();
		arrBaseColor.at(COLOR_A) = this->Base<COLOR_A>();
		return arrBaseColor;
	}

	/* set color from float array w/ alpha (in: 0.0 - 1.0, out: 0 - 255) */
	static Color FromBase4(float arrBase[4])
	{
		return Color(arrBase[0], arrBase[1], arrBase[2], arrBase[3]);
	}

	float Hue() const
	{
		if (arrColor.at(COLOR_R) == arrColor.at(COLOR_G) && arrColor.at(COLOR_G) == arrColor.at(COLOR_B))
			return 0.f;

		const float r = this->Base<COLOR_R>();
		const float g = this->Base<COLOR_G>();
		const float b = this->Base<COLOR_B>();

		const float flMax = (std::max)(r, (std::max)(g, b)), flMin = (std::min)(r, (std::min)(g, b));

		if (flMax == flMin)
			return 0.f;

		const float flDelta = flMax - flMin;
		float flHue = 0.f;

		if (flMax == r)
			flHue = (g - b) / flDelta;
		else if (flMax == g)
			flHue = 2.f + (b - r) / flDelta;
		else if (flMax == b)
			flHue = 4.f + (r - g) / flDelta;

		flHue *= 60.f;

		if (flHue < 0.f)
			flHue += 360.f;

		return flHue / 360.f;
	}

	float Saturation() const
	{
		const float r = this->Base<COLOR_R>();
		const float g = this->Base<COLOR_G>();
		const float b = this->Base<COLOR_B>();

		const float flMax = (std::max)(r, (std::max)(g, b)), flMin = (std::min)(r, (std::min)(g, b));
		const float flDelta = flMax - flMin;

		if (flMax == 0.f)
			return flDelta;

		return flDelta / flMax;
	}

	float Brightness() const
	{
		const float r = this->Base<COLOR_R>();
		const float g = this->Base<COLOR_G>();
		const float b = this->Base<COLOR_B>();

		return (std::max)(r, (std::max)(g, b));
	}

	/* return RGB color converted from HSB/HSV color */
	static Color FromHSB(float flHue, float flSaturation, float flBrightness)
	{
		const float h = std::fmodf(flHue, 1.0f) / (60.0f / 360.0f);
		const int i = static_cast<int>(h);
		const float f = h - static_cast<float>(i);
		const float p = flBrightness * (1.0f - flSaturation);
		const float q = flBrightness * (1.0f - flSaturation * f);
		const float t = flBrightness * (1.0f - flSaturation * (1.0f - f));

		float r = 0.0f, g = 0.0f, b = 0.0f;

		switch (i)
		{
		case 0:
			r = flBrightness, g = t, b = p;
			break;
		case 1:
			r = q, g = flBrightness, b = p;
			break;
		case 2:
			r = p, g = flBrightness, b = t;
			break;
		case 3:
			r = p, g = q, b = flBrightness;
			break;
		case 4:
			r = t, g = p, b = flBrightness;
			break;
		case 5:
		default:
			r = flBrightness, g = p, b = q;
			break;
		}

		return Color(r, g, b);
	}

	/* return RGB color converted from heximal input */
	static Color FromHex(unsigned uHexValue)
	{
		int iR = (uHexValue >> 24) & 0xFF;
		int iG = (uHexValue >> 16) & 0xFF;
		int iB = (uHexValue >> 8) & 0xFF;
		int iA = (uHexValue) & 0xFF;

		return Color(iR, iG, iB, iA);
	}

	/* convert color to heximal value */
	[[nodiscard]] unsigned GetHex() const
	{
		return (((arrColor[COLOR_R]) << 24) | ((arrColor[COLOR_G]) << 16) | ((arrColor[COLOR_B]) << 8) | ((arrColor[COLOR_A])));
	}

private:
	std::array<std::uint8_t, 4U> arrColor;
};

class C_ByteColor {
public:
	unsigned char r, g, b;

	C_ByteColor(float _r = 0, float _g = 0, float _b = 0) {
		r = static_cast<unsigned char>(_r);
		g = static_cast<unsigned char>(_g);
		b = static_cast<unsigned char>(_b);
	}

	C_ByteColor operator+(C_ByteColor color) {
		return C_ByteColor(r + color.r, g + color.g, b + color.b);
	}

	C_ByteColor operator+(unsigned char n) {
		return C_ByteColor(r + n, g + n, b + n);
	}

	C_ByteColor operator+=(C_ByteColor color) {
		r += color.r;
		g += color.g;
		b += color.b;

		return *this;
	}

	C_ByteColor operator+=(unsigned char n) {
		r += n;
		g += n;
		b += n;

		return *this;
	}

	C_ByteColor operator-(C_ByteColor color) {
		return C_ByteColor(r - color.r, g - color.g, b - color.b);
	}

	C_ByteColor operator-(unsigned char n) {
		return C_ByteColor(r - n, g - n, b - n);
	}

	C_ByteColor operator-=(C_ByteColor color) {
		r -= color.r;
		g -= color.g;
		b -= color.b;

		return *this;
	}

	C_ByteColor operator-=(unsigned char n) {
		r -= n;
		g -= n;
		b -= n;

		return *this;
	}

	C_ByteColor operator/(C_ByteColor color) {
		return C_ByteColor(r / color.r, g / color.g, b / color.b);
	}

	C_ByteColor operator/(unsigned char n) {
		return C_ByteColor(r / n, g / n, b / n);
	}

	C_ByteColor operator/=(C_ByteColor color) {
		r /= color.r;
		g /= color.g;
		b /= color.b;

		return *this;
	}

	C_ByteColor operator/=(unsigned char n) {
		r /= n;
		g /= n;
		b /= n;

		return *this;
	}

	C_ByteColor operator*(C_ByteColor color) {
		return C_ByteColor(r * color.r, g * color.g, b * color.b);
	}

	C_ByteColor operator*(unsigned char n) {
		return C_ByteColor(r * n, g * n, b * n);
	}

	C_ByteColor operator*=(C_ByteColor color) {
		r *= color.r;
		g *= color.g;
		b *= color.b;

		return *this;
	}

	C_ByteColor operator*=(unsigned char n) {
		r *= n;
		g *= n;
		b *= n;

		return *this;
	}

	C_ByteColor to_byte() {
		return C_ByteColor(r, g, b);
	}

	bool operator==(C_ByteColor color) {
		return r == color.r && g == color.g && b == color.b;
	}

	bool operator!=(C_ByteColor color) {
		return !(*this == color);
	}
};

class c_color {
public:
	float r, g, b, a;

	c_color(float _r = 0, float _g = 0, float _b = 0, float _a = 0) {
		r = _r;
		g = _g;
		b = _b;
		a = _a;
	}

	c_color operator+(c_color color) {
		return c_color(r + color.r, g + color.g, b + color.b, a + color.a);
	}

	c_color operator+(float n) {
		return c_color(r + n, g + n, b + n, a + n);
	}

	c_color operator+=(c_color color) {
		r += color.r;
		g += color.g;
		b += color.b;
		a += color.a;

		return *this;
	}

	c_color operator+=(float n) {
		r += n;
		g += n;
		b += n;
		a += n;

		return *this;
	}

	c_color operator-(c_color color) {
		return c_color(r - color.r, g - color.g, b - color.b, a - color.a);
	}

	c_color operator-(float n) {
		return c_color(r - n, g - n, b - n, a - n);
	}

	c_color operator-=(c_color color) {
		r -= color.r;
		g -= color.g;
		b -= color.b;
		a -= color.a;

		return *this;
	}

	c_color operator-=(float n) {
		r -= n;
		g -= n;
		b -= n;
		a -= n;

		return *this;
	}

	c_color operator/(c_color color) {
		return c_color(r / color.r, g / color.g, b / color.b, a / color.a);
	}

	c_color operator/(float n) {
		return c_color(r / n, g / n, b / n, a / n);
	}

	c_color operator/=(c_color color) {
		r /= color.r;
		g /= color.g;
		b /= color.b;
		a /= color.a;

		return *this;
	}

	c_color operator/=(float n) {
		r /= n;
		g /= n;
		b /= n;
		a /= n;

		return *this;
	}

	c_color operator*(c_color color) {
		return c_color(r * color.r, g * color.g, b * color.b, a * color.a);
	}

	c_color operator*(float n) {
		return c_color(r * n, g * n, b * n, a * n);
	}

	c_color operator*=(c_color color) {
		r *= color.r;
		g *= color.g;
		b *= color.b;
		a *= color.a;

		return *this;
	}

	c_color operator*=(float n) {
		r *= n;
		g *= n;
		b *= n;
		a *= n;

		return *this;
	}

	bool operator==(c_color color) {
		return r == color.r && g == color.g && b == color.b && a == color.a;
	}

	bool operator!=(c_color color) {
		return !(*this == color);
	}

	c_color convert_from_hsv_to_color() {
		r /= 255.f;
		g /= 255.f;
		b /= 255.f;
		a /= 255.f;
		return *this;
	}

	c_color lerp(c_color& other, float step) {
		c_color lerped_color{
			std::lerp(r, other.r, step),
			std::lerp(g, other.g, step),
			std::lerp(b, other.b, step),
			std::lerp(a, other.a, step)
		};

		return lerped_color;
	}

	C_ByteColor to_byte() {
		return C_ByteColor(r, g, b);
	}

	ImColor im()
	{
		return ImColor(r, g, b, a);
	}
};

struct Color_t
{
	Color_t() = default;

	// 8-bit color constructor (in: [0 .. 255])
	constexpr Color_t(const std::uint8_t r, const std::uint8_t g, const std::uint8_t b, const std::uint8_t a = 255) :
		r(r), g(g), b(b), a(a) {
	}

	// 8-bit color constructor (in: [0 .. 255])
	constexpr Color_t(const int r, const int g, const int b, const int a = 255) :
		r(static_cast<std::uint8_t>(r)), g(static_cast<std::uint8_t>(g)), b(static_cast<std::uint8_t>(b)), a(static_cast<std::uint8_t>(a)) {
	}

	// 8-bit array color constructor (in: [0.0 .. 1.0])
	explicit constexpr Color_t(const std::uint8_t arrColor[4]) :
		r(arrColor[COLOR_R]), g(arrColor[COLOR_G]), b(arrColor[COLOR_B]), a(arrColor[COLOR_A]) {
	}

	// 32-bit packed color constructor (in: 0x00000000 - 0xFFFFFFFF)
	explicit constexpr Color_t(const ImU32 uPackedColor) :
		r(static_cast<std::uint8_t>((uPackedColor >> IM_COL32_R_SHIFT) & 0xFF)), g(static_cast<std::uint8_t>((uPackedColor >> IM_COL32_G_SHIFT) & 0xFF)), b(static_cast<std::uint8_t>((uPackedColor >> IM_COL32_B_SHIFT) & 0xFF)), a(static_cast<std::uint8_t>((uPackedColor >> IM_COL32_A_SHIFT) & 0xFF)) {
	}

	// 32-bit color constructor (in: [0.0 .. 1.0])
	constexpr Color_t(const float r, const float g, const float b, const float a = 1.0f) :
		r(static_cast<std::uint8_t>(r * 255.f)), g(static_cast<std::uint8_t>(g * 255.f)), b(static_cast<std::uint8_t>(b * 255.f)), a(static_cast<std::uint8_t>(a * 255.f)) {
	}


	/// @returns: 32-bit packed integer representation of color
	[[nodiscard]] constexpr ImU32 GetU32(const float flAlphaMultiplier = 1.0f) const
	{
		return IM_COL32(r, g, b, a * flAlphaMultiplier);
	}

	/// @return: converted color to imgui vector
	[[nodiscard]] ImVec4 GetVec4(const float flAlphaMultiplier = 1.0f) const
	{
		return ImVec4(this->Base<COLOR_R>(), this->Base<COLOR_G>(), this->Base<COLOR_B>(), this->Base<COLOR_A>() * flAlphaMultiplier);
	}

	// Method to directly return a pointer to the color components in float
	[[nodiscard]] float* GetFloat() const {
		// Static array to store float values
		float floatArrBase[4];
		floatArrBase[COLOR_R] = static_cast<float>(r) / 255.f;
		floatArrBase[COLOR_G] = static_cast<float>(g) / 255.f;
		floatArrBase[COLOR_B] = static_cast<float>(b) / 255.f;
		floatArrBase[COLOR_A] = static_cast<float>(a) / 255.f;
		return floatArrBase;
	}

	[[nodiscard]] std::array<int, 4> GetIntRGBA() const {
		// Static array to store float values
		float floatArrBase[4];
		floatArrBase[0] = static_cast<float>(r) / 255.f;
		floatArrBase[1] = static_cast<float>(g) / 255.f;
		floatArrBase[2] = static_cast<float>(b) / 255.f;
		floatArrBase[3] = static_cast<float>(a) / 255.f;

		// Convert float values back to integer RGB values
		std::array<int, 4> intRGBA = {
			static_cast<int>(floatArrBase[0] * 255),
			static_cast<int>(floatArrBase[1] * 255),
			static_cast<int>(floatArrBase[2] * 255),
			static_cast<int>(floatArrBase[3] * 255)
		};

		return intRGBA;
	}


	std::uint8_t& operator[](const std::uint8_t nIndex)
	{
		return reinterpret_cast<std::uint8_t*>(this)[nIndex];
	}

	const std::uint8_t& operator[](const std::uint8_t nIndex) const
	{
		return reinterpret_cast<const std::uint8_t*>(this)[nIndex];
	}

	bool operator==(const Color_t& colSecond) const
	{
		return (std::bit_cast<std::uint32_t>(*this) == std::bit_cast<std::uint32_t>(colSecond));
	}

	bool operator!=(const Color_t& colSecond) const
	{
		return (std::bit_cast<std::uint32_t>(*this) != std::bit_cast<std::uint32_t>(colSecond));
	}

	/// @returns: copy of color with certain R/G/B/A component changed to given value
	template <std::size_t N>
	Color_t Set(const std::uint8_t nValue) const
	{

		Color_t colCopy = *this;
		colCopy[N] = nValue;
		return colCopy;
	}
	Color_t Set(const ImVec4& imColor) const
	{
		return { static_cast<std::uint8_t>(imColor.x * 255.f),
				 static_cast<std::uint8_t>(imColor.y * 255.f),
				 static_cast<std::uint8_t>(imColor.z * 255.f),
				 static_cast<std::uint8_t>(imColor.w * 255.f) };
	}

	// Method to set the alpha to the product of current alpha and an additional alpha multiplier
	uint8_t SetAlphaM(const float alphaMultiplier) const
	{
		// Make a copy of the current color
		Color_t colCopy = *this;

		// Multiply the alpha value by the alpha multiplier
		colCopy.a = static_cast<std::uint8_t>(alphaMultiplier);
		// Return the modified color
		return colCopy.a;
	}

	/// @returns: copy of color with certain R/G/B/A component multiplied by given value
	template <std::size_t N>
	[[nodiscard]] Color_t Multiplier(const float flValue) const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "color component index is out of range");

		Color_t colCopy = *this;
		colCopy[N] = static_cast<std::uint8_t>(static_cast<float>(colCopy[N]) * flValue);
		return colCopy;
	}

	/// @returns: copy of color with certain R/G/B/A component divided by given value
	template <std::size_t N>
	[[nodiscard]] Color_t Divider(const int iValue) const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "color component index is out of range");

		Color_t colCopy = *this;
		colCopy[N] /= iValue;
		return colCopy;
	}

	/// @returns: certain R/G/B/A float value (in: [0 .. 255], out: [0.0 .. 1.0])
	template <std::size_t N>
	[[nodiscard]] float Base() const
	{
		static_assert(N >= COLOR_R && N <= COLOR_A, "color component index is out of range");
		return reinterpret_cast<const std::uint8_t*>(this)[N] / 255.f;
	}

	/// @param[out] arrBase output array of R/G/B color components converted to float (in: [0 .. 255], out: [0.0 .. 1.0])
	constexpr void Base(float(&arrBase)[3]) const
	{
		arrBase[COLOR_R] = static_cast<float>(r) / 255.f;
		arrBase[COLOR_G] = static_cast<float>(g) / 255.f;
		arrBase[COLOR_B] = static_cast<float>(b) / 255.f;
	}

	/// @returns: color created from float[3] array (in: [0.0 .. 1.0], out: [0 .. 255])
	static Color_t FromBase3(const float arrBase[3])
	{
		return { arrBase[0], arrBase[1], arrBase[2] };
	}

	constexpr std::array<float, 4> Basef() const
	{
		return {
			static_cast<float>(r) / 255.f,
			static_cast<float>(g) / 255.f,
			static_cast<float>(b) / 255.f,
			static_cast<float>(a) / 255.f
		};
	}
	/// @param[out] arrBase output array of R/G/B/A color components converted to float (in: [0 .. 255], out: [0.0 .. 1.0])
	constexpr void BaseAlpha(float(&arrBase)[4]) const
	{
		arrBase[COLOR_R] = static_cast<float>(r) / 255.f;
		arrBase[COLOR_G] = static_cast<float>(g) / 255.f;
		arrBase[COLOR_B] = static_cast<float>(b) / 255.f;
		arrBase[COLOR_A] = static_cast<float>(a) / 255.f;
	}

	/// @returns : color created from float[3] array (in: [0.0 .. 1.0], out: [0 .. 255])
	static Color_t FromBase4(const float arrBase[4])
	{
		return { arrBase[COLOR_R], arrBase[COLOR_G], arrBase[COLOR_B], arrBase[COLOR_A] };
	}

	/// @param[out] arrHSB output array of HSB/HSV color converted from RGB color
	void ToHSB(float(&arrHSB)[3]) const
	{
		float arrBase[3] = {};
		Base(arrBase);
	}

	/// @returns: RGB color converted from HSB/HSV color
	static Color_t FromHSB(const float flHue, const float flSaturation, const float flBrightness, const float flAlpha = 1.0f)
	{
		constexpr float flHueRange = (60.0f / 360.0f);
		const float flHuePrime = std::fmodf(flHue, 1.0f) / flHueRange;
		const int iRoundHuePrime = static_cast<int>(flHuePrime);
		const float flDelta = flHuePrime - static_cast<float>(iRoundHuePrime);

		const float p = flBrightness * (1.0f - flSaturation);
		const float q = flBrightness * (1.0f - flSaturation * flDelta);
		const float t = flBrightness * (1.0f - flSaturation * (1.0f - flDelta));

		float flRed, flGreen, flBlue;
		switch (iRoundHuePrime)
		{
		case 0:
			flRed = flBrightness;
			flGreen = t;
			flBlue = p;
			break;
		case 1:
			flRed = q;
			flGreen = flBrightness;
			flBlue = p;
			break;
		case 2:
			flRed = p;
			flGreen = flBrightness;
			flBlue = t;
			break;
		case 3:
			flRed = p;
			flGreen = q;
			flBlue = flBrightness;
			break;
		case 4:
			flRed = t;
			flGreen = p;
			flBlue = flBrightness;
			break;
		default:
			flRed = flBrightness;
			flGreen = p;
			flBlue = q;
			break;
		}

		return { flRed, flGreen, flBlue, flAlpha };
	}

	std::uint8_t r = 0U, g = 0U, b = 0U, a = 0U;
};

class C_EnvSky {
public:
	schema(bool, m_bStartDisabled, "C_EnvSky->m_bStartDisabled");
	schema(Color, m_vTintColor, "C_EnvSky->m_vTintColor");
	schema(Color, m_vTintColorLightingOnly, "C_EnvSky->m_vTintColorLightingOnly");
	schema(float, m_flBrightnessScale, "C_EnvSky->m_flBrightnessScale");
	schema(int, m_nFogType, "C_EnvSky->m_nFogType");
	schema(float, m_flFogMinStart, "C_EnvSky->m_flFogMinStart");
	schema(float, m_flFogMinEnd, "C_EnvSky->m_flFogMinEnd");
	schema(float, m_flFogMaxStart, "C_EnvSky->m_flFogMaxStart");
	schema(float, m_flFogMaxEnd, "C_EnvSky->m_flFogMaxEnd");
	schema(bool, m_bEnabled, "C_EnvSky->m_bEnabled");
};