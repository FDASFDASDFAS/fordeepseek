#pragma once

#include "../runtime/result.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace tw::commands
{
    struct Angles final
    {
        float pitch{};
        float yaw{};
        float roll{};

        [[nodiscard]] friend constexpr bool operator==(const Angles&, const Angles&) noexcept = default;
    };

    struct Movement final
    {
        float forward{};
        float side{};
        float up{};

        [[nodiscard]] friend constexpr bool operator==(const Movement&, const Movement&) noexcept = default;
    };

    struct CommandDraft final
    {
        std::uint64_t sequence{};
        Angles view{};
        Movement movement{};
        std::uint64_t buttons{};

        [[nodiscard]] friend constexpr bool operator==(const CommandDraft&, const CommandDraft&) noexcept = default;
    };

    enum class IntentKind : std::uint8_t
    {
        User,
        Movement,
        EdgeSafety,
        QuickPeekReturn,
        Stance,
        AntiAim,
        Aim,
        Fire
    };

    struct CommandIntent final
    {
        IntentKind kind{};
        std::uint16_t priority{};
        std::uint32_t stableOrdinal{};
        std::optional<Angles> view{};
        std::optional<Movement> movement{};
        std::uint64_t setButtons{};
        std::uint64_t clearButtons{};
        bool manualMovement{};
        bool requestsShot{};
    };

    struct ResolvedCommand final
    {
        CommandDraft command{};
        bool shotCommitted{};
    };

    class ICommandRuntime
    {
    public:
        virtual ~ICommandRuntime() = default;
        [[nodiscard]] virtual runtime::Result<CommandDraft> capture() noexcept = 0;
        [[nodiscard]] virtual runtime::Status commit(const ResolvedCommand& command) noexcept = 0;
    };

    class IPredictionRuntime
    {
    public:
        virtual ~IPredictionRuntime() = default;
        [[nodiscard]] virtual runtime::Status begin(const CommandDraft& command) noexcept = 0;
        virtual void end() noexcept = 0;
    };

    // Event-scoped adapter used while the live commit boundary is observational only.
    class ObservedCommandRuntime final : public ICommandRuntime
    {
    public:
        explicit ObservedCommandRuntime(CommandDraft observed) noexcept : observed_(observed) {}

        [[nodiscard]] runtime::Result<CommandDraft> capture() noexcept override;
        [[nodiscard]] runtime::Status commit(const ResolvedCommand& command) noexcept override;
        [[nodiscard]] std::uint32_t captureCount() const noexcept { return captureCount_; }
        [[nodiscard]] std::uint32_t commitCount() const noexcept { return commitCount_; }

    private:
        CommandDraft observed_{};
        std::uint32_t captureCount_{};
        std::uint32_t commitCount_{};
    };

    using CommandApply = runtime::Status(*)(void* context, const ResolvedCommand& command) noexcept;

    // Event-scoped binding for a validated live command boundary.
    class BoundCommandRuntime final : public ICommandRuntime
    {
    public:
        BoundCommandRuntime(CommandDraft observed, void* context, CommandApply apply) noexcept
            : observed_(observed), context_(context), apply_(apply) {}

        [[nodiscard]] runtime::Result<CommandDraft> capture() noexcept override;
        [[nodiscard]] runtime::Status commit(const ResolvedCommand& command) noexcept override;
        [[nodiscard]] std::uint32_t captureCount() const noexcept { return captureCount_; }
        [[nodiscard]] std::uint32_t commitCount() const noexcept { return commitCount_; }

    private:
        CommandDraft observed_{};
        void* context_{};
        CommandApply apply_{};
        std::uint32_t captureCount_{};
        std::uint32_t commitCount_{};
    };

    class UnsupportedPredictionRuntime final : public IPredictionRuntime
    {
    public:
        [[nodiscard]] runtime::Status begin(const CommandDraft&) noexcept override;
        void end() noexcept override {}
    };

    enum class CommandProbeState : std::uint8_t
    {
        AwaitingTarget,
        Observing,
        ReadOnlyStable,
        Rejected
    };

    struct CommandProbeSample final
    {
        std::uint64_t threadToken{};
        std::uint64_t sequence{};
        bool sequencePresent{};
        bool moveCrcPresent{};
        bool passThroughSucceeded{};
        bool beforeAvailable{};
        std::uint64_t beforeSequence{};
        bool beforeSequencePresent{};
        bool beforeMoveCrcPresent{};
    };

    struct MoveCrcBoundarySample final
    {
        std::uint64_t threadToken{};
        bool beforeAvailable{};
        bool beforeMoveCrcPresent{};
        bool afterAvailable{};
        bool afterMoveCrcPresent{};
        bool commitSucceeded{};
        bool movementMutated{};
        bool buttonsMutated{};
        std::uint16_t unavailableOperation{};
    };

    struct ButtonTransitionProbeSample final
    {
        bool statesMatch{};
        bool jumpPressed{};
        bool jumpTransitionPresent{};
    };

    struct ViewHistoryProbeSample final
    {
        std::uint32_t entries{};
        std::uint32_t entriesWithView{};
        bool firstMatchesBase{};
        bool lastMatchesBase{};
        bool unchangedAcrossFinalization{};
        bool truncated{};
    };

    struct MovementSnapshotProbeSample final
    {
        bool available{};
        bool standardWalkType{};
        std::int32_t rawMoveType{};
        std::uint16_t unavailableOperation{};
    };

    struct CommandProbeSnapshot final
    {
        CommandProbeState state{ CommandProbeState::AwaitingTarget };
        std::uint32_t imageTimestamp{};
        std::uint32_t imageSize{};
        std::uint64_t threadToken{};
        std::uint64_t samples{};
        std::uint64_t passThroughs{};
        std::uint64_t advancingSequences{};
        std::uint64_t moveCrcSamples{};
        std::uint64_t unavailableSamples{};
        std::uint64_t beforeSamples{};
        std::uint64_t beforeMoveCrcSamples{};
        std::uint64_t matchingAcrossOriginal{};
        std::uint64_t advancingAcrossOriginal{};
        std::uint16_t lastOperation{};
        bool inputRttiFound{};
        std::uint32_t inputVtableCount{};
        std::uint32_t largestInputVtable{};
        std::uint32_t firstInputVtableRva{};
        bool commandTargetValidated{};
        std::uint32_t commandTargetRva{};
        bool moveCrcTargetValidated{};
        std::uint32_t moveCrcTargetRva{};
        bool moveCrcBoundaryStable{};
        std::uint64_t moveCrcBoundarySamples{};
        std::uint64_t moveCrcBeforeSamples{};
        std::uint64_t moveCrcBeforePresentSamples{};
        std::uint64_t moveCrcAfterSamples{};
        std::uint64_t moveCrcAfterPresentSamples{};
        std::uint64_t moveCrcTransitions{};
        std::uint64_t moveCrcUnavailableSamples{};
        std::uint64_t preCrcCommits{};
        std::uint64_t preCrcCommitFailures{};
        std::uint64_t movementCommits{};
        std::uint64_t buttonCommits{};
        std::uint16_t lastMoveCrcOperation{};
        bool jumpTransitionStable{};
        std::uint64_t buttonStateSamples{};
        std::uint64_t matchingButtonStateSamples{};
        std::uint64_t jumpPressedSamples{};
        std::uint64_t jumpTransitionSamples{};
        bool viewHistoryReadStable{};
        std::uint64_t viewHistorySamples{};
        std::uint64_t viewHistoryEntries{};
        std::uint64_t historyEntriesWithView{};
        std::uint64_t firstHistoryMatchesBase{};
        std::uint64_t lastHistoryMatchesBase{};
        std::uint64_t unchangedHistorySamples{};
        std::uint64_t changedHistorySamples{};
        std::uint64_t truncatedHistorySamples{};
        bool movementSnapshotStable{};
        std::uint64_t movementSnapshotSamples{};
        std::uint64_t movementSnapshotUnavailable{};
        std::uint64_t standardWalkTypeSamples{};
        std::int32_t lastRawMoveType{};
        std::uint16_t lastMovementSnapshotOperation{};
    };

    class CommandRuntimeProbe final
    {
    public:
        void setImageIdentity(std::uint32_t timestamp, std::uint32_t imageSize) noexcept;
        void setInputRttiEvidence(bool typeFound, std::uint32_t vtableCount, std::uint32_t largestVtable,
            std::uint32_t firstVtableRva) noexcept;
        void setCommandTargetEvidence(bool validated, std::uint32_t targetRva) noexcept;
        void setMoveCrcTargetEvidence(bool validated, std::uint32_t targetRva) noexcept;
        void resetSession() noexcept;
        void record(CommandProbeSample sample) noexcept;
        void recordMoveCrcBoundary(MoveCrcBoundarySample sample) noexcept;
        void recordButtonTransition(ButtonTransitionProbeSample sample) noexcept;
        void recordViewHistory(ViewHistoryProbeSample sample) noexcept;
        void recordMovementSnapshot(MovementSnapshotProbeSample sample) noexcept;
        void recordUnavailable(std::uint16_t operation) noexcept;
        [[nodiscard]] CommandProbeSnapshot snapshot() const noexcept;

    private:
        static constexpr std::uint64_t requiredSamples = 16;
        static constexpr std::uint64_t requiredSequenceAdvances = 8;
        static constexpr std::uint64_t requiredMoveCrcTransitions = 8;
        static constexpr std::uint64_t requiredJumpSamples = 2;

        mutable std::mutex mutex_{};
        CommandProbeSnapshot snapshot_{};
        std::uint64_t lastSequence_{};
        bool hasLastSequence_{};
        std::uint64_t moveCrcThreadToken_{};
    };

    class PredictionScope final
    {
    public:
        PredictionScope(IPredictionRuntime& runtime, const CommandDraft& command) noexcept;
        ~PredictionScope();
        PredictionScope(const PredictionScope&) = delete;
        PredictionScope& operator=(const PredictionScope&) = delete;
        [[nodiscard]] bool active() const noexcept { return active_; }
        [[nodiscard]] const runtime::Status& status() const noexcept { return status_; }

    private:
        IPredictionRuntime& runtime_;
        runtime::Status status_{};
        bool active_{};
    };

    class CommandResolver final
    {
    public:
        [[nodiscard]] runtime::Result<ResolvedCommand> resolve(CommandDraft draft, std::vector<CommandIntent> intents) const;
    };

    class CommandPipeline final
    {
    public:
        CommandPipeline(ICommandRuntime& commandRuntime, IPredictionRuntime& predictionRuntime) noexcept
            : commandRuntime_(commandRuntime), predictionRuntime_(predictionRuntime) {}

        [[nodiscard]] runtime::Result<ResolvedCommand> run(std::vector<CommandIntent> intents) noexcept;

    private:
        ICommandRuntime& commandRuntime_;
        IPredictionRuntime& predictionRuntime_;
        CommandResolver resolver_{};
    };
}
