#include "command_pipeline.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tw::commands
{
    namespace
    {
        float normalizeYaw(float value) noexcept
        {
            value = std::remainder(value, 360.0f);
            return value == -180.0f ? 180.0f : value;
        }

        bool finite(const Angles& angles) noexcept
        {
            return std::isfinite(angles.pitch) && std::isfinite(angles.yaw) && std::isfinite(angles.roll);
        }

        bool finite(const Movement& movement) noexcept
        {
            return std::isfinite(movement.forward) && std::isfinite(movement.side) && std::isfinite(movement.up);
        }
    }

    runtime::Result<CommandDraft> ObservedCommandRuntime::capture() noexcept
    {
        if (captureCount_ != 0)
            return runtime::Result<CommandDraft>::failure({ runtime::StatusCode::InvalidState, runtime::Subsystem::Commands, 4, 0 });
        ++captureCount_;
        return runtime::Result<CommandDraft>::success(observed_);
    }

    runtime::Status ObservedCommandRuntime::commit(const ResolvedCommand& command) noexcept
    {
        if (captureCount_ != 1 || commitCount_ != 0)
            return runtime::Status::failure({ runtime::StatusCode::InvalidState, runtime::Subsystem::Commands, 5, 0 });
        ++commitCount_;
        if (command.shotCommitted || command.command != observed_)
            return runtime::Status::failure({ runtime::StatusCode::Unsupported, runtime::Subsystem::Commands, 6, 0 });
        return runtime::Status::success();
    }

    runtime::Result<CommandDraft> BoundCommandRuntime::capture() noexcept
    {
        if (captureCount_ != 0)
            return runtime::Result<CommandDraft>::failure({ runtime::StatusCode::InvalidState, runtime::Subsystem::Commands, 7, 0 });
        ++captureCount_;
        return runtime::Result<CommandDraft>::success(observed_);
    }

    runtime::Status BoundCommandRuntime::commit(const ResolvedCommand& command) noexcept
    {
        if (captureCount_ != 1 || commitCount_ != 0)
            return runtime::Status::failure({ runtime::StatusCode::InvalidState, runtime::Subsystem::Commands, 8, 0 });
        ++commitCount_;
        if (command.command.sequence != observed_.sequence)
            return runtime::Status::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Commands, 9, 0 });
        if (!context_ || !apply_)
            return runtime::Status::failure({ runtime::StatusCode::Unavailable, runtime::Subsystem::Commands, 10, 0 });
        return apply_(context_, command);
    }

    runtime::Status UnsupportedPredictionRuntime::begin(const CommandDraft&) noexcept
    {
        return runtime::Status::failure({ runtime::StatusCode::Unsupported, runtime::Subsystem::Prediction, 1, 0 });
    }

    void CommandRuntimeProbe::setImageIdentity(const std::uint32_t timestamp, const std::uint32_t imageSize) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        snapshot_.imageTimestamp = timestamp;
        snapshot_.imageSize = imageSize;
    }

    void CommandRuntimeProbe::setInputRttiEvidence(const bool typeFound, const std::uint32_t vtableCount,
        const std::uint32_t largestVtable, const std::uint32_t firstVtableRva) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        snapshot_.inputRttiFound = typeFound;
        snapshot_.inputVtableCount = vtableCount;
        snapshot_.largestInputVtable = largestVtable;
        snapshot_.firstInputVtableRva = firstVtableRva;
    }

    void CommandRuntimeProbe::setCommandTargetEvidence(const bool validated, const std::uint32_t targetRva) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        snapshot_.commandTargetValidated = validated;
        snapshot_.commandTargetRva = targetRva;
    }

    void CommandRuntimeProbe::setMoveCrcTargetEvidence(const bool validated, const std::uint32_t targetRva) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        snapshot_.moveCrcTargetValidated = validated;
        snapshot_.moveCrcTargetRva = targetRva;
    }

    void CommandRuntimeProbe::resetSession() noexcept
    {
        std::scoped_lock lock{ mutex_ };
        const std::uint32_t timestamp = snapshot_.imageTimestamp;
        const std::uint32_t imageSize = snapshot_.imageSize;
        const bool inputRttiFound = snapshot_.inputRttiFound;
        const std::uint32_t inputVtableCount = snapshot_.inputVtableCount;
        const std::uint32_t largestInputVtable = snapshot_.largestInputVtable;
        const std::uint32_t firstInputVtableRva = snapshot_.firstInputVtableRva;
        const bool commandTargetValidated = snapshot_.commandTargetValidated;
        const std::uint32_t commandTargetRva = snapshot_.commandTargetRva;
        const bool moveCrcTargetValidated = snapshot_.moveCrcTargetValidated;
        const std::uint32_t moveCrcTargetRva = snapshot_.moveCrcTargetRva;
        snapshot_ = {};
        snapshot_.imageTimestamp = timestamp;
        snapshot_.imageSize = imageSize;
        snapshot_.inputRttiFound = inputRttiFound;
        snapshot_.inputVtableCount = inputVtableCount;
        snapshot_.largestInputVtable = largestInputVtable;
        snapshot_.firstInputVtableRva = firstInputVtableRva;
        snapshot_.commandTargetValidated = commandTargetValidated;
        snapshot_.commandTargetRva = commandTargetRva;
        snapshot_.moveCrcTargetValidated = moveCrcTargetValidated;
        snapshot_.moveCrcTargetRva = moveCrcTargetRva;
        lastSequence_ = 0;
        hasLastSequence_ = false;
        moveCrcThreadToken_ = 0;
    }

    void CommandRuntimeProbe::record(const CommandProbeSample sample) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        if (snapshot_.state == CommandProbeState::Rejected)
            return;

        if (snapshot_.state == CommandProbeState::AwaitingTarget)
        {
            snapshot_.state = CommandProbeState::Observing;
            snapshot_.threadToken = sample.threadToken;
        }
        else if (snapshot_.threadToken != sample.threadToken)
        {
            snapshot_.state = CommandProbeState::Rejected;
            snapshot_.lastOperation = 20;
            return;
        }

        ++snapshot_.samples;
        if (!sample.passThroughSucceeded)
        {
            snapshot_.state = CommandProbeState::Rejected;
            snapshot_.lastOperation = 21;
            return;
        }

        ++snapshot_.passThroughs;
        if (sample.moveCrcPresent)
            ++snapshot_.moveCrcSamples;
        if (sample.beforeAvailable)
        {
            ++snapshot_.beforeSamples;
            if (sample.beforeMoveCrcPresent)
                ++snapshot_.beforeMoveCrcSamples;
            if (sample.beforeSequencePresent && sample.sequencePresent)
            {
                if (sample.beforeSequence == sample.sequence)
                    ++snapshot_.matchingAcrossOriginal;
                else if (sample.sequence > sample.beforeSequence)
                    ++snapshot_.advancingAcrossOriginal;
            }
        }
        if (sample.sequencePresent)
        {
            if (hasLastSequence_ && sample.sequence > lastSequence_)
                ++snapshot_.advancingSequences;
            lastSequence_ = sample.sequence;
            hasLastSequence_ = true;
        }

        if (snapshot_.passThroughs >= requiredSamples && snapshot_.advancingSequences >= requiredSequenceAdvances)
            snapshot_.state = CommandProbeState::ReadOnlyStable;
    }

    void CommandRuntimeProbe::recordMoveCrcBoundary(const MoveCrcBoundarySample sample) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        if (snapshot_.state == CommandProbeState::Rejected)
            return;

		if (moveCrcThreadToken_ == 0)
			moveCrcThreadToken_ = sample.threadToken;
		else if (moveCrcThreadToken_ != sample.threadToken)
		{
            snapshot_.state = CommandProbeState::Rejected;
            snapshot_.lastOperation = 22;
            return;
        }

        ++snapshot_.moveCrcBoundarySamples;
        if (sample.beforeAvailable)
        {
            ++snapshot_.moveCrcBeforeSamples;
            if (sample.beforeMoveCrcPresent)
                ++snapshot_.moveCrcBeforePresentSamples;
        }
        if (sample.afterAvailable)
        {
            ++snapshot_.moveCrcAfterSamples;
            if (sample.afterMoveCrcPresent)
                ++snapshot_.moveCrcAfterPresentSamples;
        }
        if (sample.beforeAvailable && sample.afterAvailable && !sample.beforeMoveCrcPresent && sample.afterMoveCrcPresent)
            ++snapshot_.moveCrcTransitions;
        if (!sample.beforeAvailable || !sample.afterAvailable)
        {
            ++snapshot_.moveCrcUnavailableSamples;
            snapshot_.lastMoveCrcOperation = sample.unavailableOperation;
        }
        if (sample.beforeAvailable)
        {
            if (sample.commitSucceeded)
                ++snapshot_.preCrcCommits;
            else
                ++snapshot_.preCrcCommitFailures;
        }
        if (sample.commitSucceeded && sample.movementMutated)
            ++snapshot_.movementCommits;
        if (sample.commitSucceeded && sample.buttonsMutated)
            ++snapshot_.buttonCommits;

        snapshot_.moveCrcBoundaryStable =
            snapshot_.moveCrcBeforeSamples >= requiredSamples &&
            snapshot_.moveCrcTransitions >= requiredMoveCrcTransitions &&
            snapshot_.moveCrcBeforePresentSamples == 0 &&
            snapshot_.moveCrcAfterSamples == snapshot_.moveCrcAfterPresentSamples &&
            snapshot_.preCrcCommits >= requiredSamples &&
            snapshot_.preCrcCommitFailures == 0;
    }

    void CommandRuntimeProbe::recordButtonTransition(const ButtonTransitionProbeSample sample) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        ++snapshot_.buttonStateSamples;
        if (sample.statesMatch)
            ++snapshot_.matchingButtonStateSamples;
        if (sample.jumpPressed)
            ++snapshot_.jumpPressedSamples;
        if (sample.jumpTransitionPresent)
            ++snapshot_.jumpTransitionSamples;
        // matchingButtonStateSamples (native CInButtonState vs. buttons_pb()) is tracked as a
        // diagnostic only: live evidence shows the two disagree on a persistent fraction of samples
        // regardless of tick state, so requiring full agreement here would keep this permanently
        // unstable. Stability instead rests on buttons_pb() alone: enough samples, enough observed
        // presses, and at least one observed rising/falling edge.
        snapshot_.jumpTransitionStable =
            snapshot_.buttonStateSamples >= requiredSamples &&
            snapshot_.jumpPressedSamples >= requiredJumpSamples &&
            snapshot_.jumpTransitionSamples != 0;
    }

    void CommandRuntimeProbe::recordViewHistory(const ViewHistoryProbeSample sample) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        ++snapshot_.viewHistorySamples;
        snapshot_.viewHistoryEntries += sample.entries;
        snapshot_.historyEntriesWithView += sample.entriesWithView;
        if (sample.firstMatchesBase)
            ++snapshot_.firstHistoryMatchesBase;
        if (sample.lastMatchesBase)
            ++snapshot_.lastHistoryMatchesBase;
        if (sample.unchangedAcrossFinalization)
            ++snapshot_.unchangedHistorySamples;
        else
            ++snapshot_.changedHistorySamples;
        if (sample.truncated)
            ++snapshot_.truncatedHistorySamples;
        snapshot_.viewHistoryReadStable =
            snapshot_.viewHistorySamples >= requiredSamples &&
            snapshot_.unchangedHistorySamples == snapshot_.viewHistorySamples &&
            snapshot_.truncatedHistorySamples == 0;
    }

    void CommandRuntimeProbe::recordMovementSnapshot(const MovementSnapshotProbeSample sample) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        snapshot_.lastRawMoveType = sample.rawMoveType;
        if (!sample.available)
        {
            ++snapshot_.movementSnapshotUnavailable;
            snapshot_.lastMovementSnapshotOperation = sample.unavailableOperation;
            return;
        }
        ++snapshot_.movementSnapshotSamples;
        if (sample.standardWalkType)
            ++snapshot_.standardWalkTypeSamples;
        snapshot_.movementSnapshotStable =
            snapshot_.movementSnapshotSamples >= requiredSamples &&
            snapshot_.standardWalkTypeSamples >= requiredSequenceAdvances;
    }

    void CommandRuntimeProbe::recordUnavailable(const std::uint16_t operation) noexcept
    {
        std::scoped_lock lock{ mutex_ };
        ++snapshot_.unavailableSamples;
        snapshot_.lastOperation = operation;
    }

    CommandProbeSnapshot CommandRuntimeProbe::snapshot() const noexcept
    {
        std::scoped_lock lock{ mutex_ };
        return snapshot_;
    }

    PredictionScope::PredictionScope(IPredictionRuntime& runtime, const CommandDraft& command) noexcept
        : runtime_(runtime), status_(runtime.begin(command)), active_(status_.ok())
    {
    }

    PredictionScope::~PredictionScope()
    {
        if (active_)
            runtime_.end();
    }

    runtime::Result<ResolvedCommand> CommandResolver::resolve(CommandDraft draft, std::vector<CommandIntent> intents) const
    {
        if (!finite(draft.view) || !finite(draft.movement))
            return runtime::Result<ResolvedCommand>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Commands, 1, 0 });

        // An empty policy set is an exact semantic pass-through, not a normalization request.
        if (intents.empty())
            return runtime::Result<ResolvedCommand>::success({ draft, false });

        // Normalize before capturing the movement-correction baseline so an intent set that never
        // touches view yields a bit-exact zero yaw delta instead of a near-zero one from re-normalizing
        // an out-of-range incoming angle (e.g. 540 degrees) against its already-normalized result.
        draft.view.pitch = std::clamp(draft.view.pitch, -89.0f, 89.0f);
        draft.view.yaw = normalizeYaw(draft.view.yaw);
        draft.view.roll = 0.0f;

        std::stable_sort(intents.begin(), intents.end(), [](const CommandIntent& left, const CommandIntent& right)
        {
            if (left.priority != right.priority)
                return left.priority < right.priority;
            return left.stableOrdinal < right.stableOrdinal;
        });

        const bool manualMovement = std::any_of(intents.begin(), intents.end(), [](const CommandIntent& intent)
        {
            return intent.manualMovement;
        });
        const bool shotRequested = std::any_of(intents.begin(), intents.end(), [](const CommandIntent& intent)
        {
            return intent.requestsShot;
        });
        const float originalYaw = draft.view.yaw;

        for (const auto& intent : intents)
        {
            if (manualMovement && intent.kind == IntentKind::QuickPeekReturn)
                continue;
            if (shotRequested && intent.kind == IntentKind::AntiAim)
                continue;
            if (intent.view)
            {
                if (!finite(*intent.view))
                    return runtime::Result<ResolvedCommand>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Commands, 2, 0 });
                draft.view = *intent.view;
            }
            if (intent.movement)
            {
                if (!finite(*intent.movement))
                    return runtime::Result<ResolvedCommand>::failure({ runtime::StatusCode::InvalidArgument, runtime::Subsystem::Commands, 3, 0 });
                draft.movement = *intent.movement;
            }
            draft.buttons &= ~intent.clearButtons;
            draft.buttons |= intent.setButtons;
        }

        draft.view.pitch = std::clamp(draft.view.pitch, -89.0f, 89.0f);
        draft.view.yaw = normalizeYaw(draft.view.yaw);
        draft.view.roll = 0.0f;
        const float yawDelta = (draft.view.yaw - originalYaw) * 0.01745329251994329577f;
        const float cosine = std::cos(yawDelta);
        const float sine = std::sin(yawDelta);
        // Pure rotation: forward/side here are CS2's raw wire-format move-speed values (order of
        // hundreds), not a normalized -1..1 unit vector, so clamping the result to [-1, 1] previously
        // crushed real movement to a crawl on every command that carried any intent at all -- including
        // view-only intents like Aim that never meant to touch movement. Rotation preserves vector
        // magnitude on its own; a feature that wants to bound its own output (see scaleMovementSpeed)
        // is responsible for doing so against the real value, not an assumed unit scale.
        const Movement originalMovement = draft.movement;
        draft.movement.forward = cosine * originalMovement.forward + sine * originalMovement.side;
        draft.movement.side = -sine * originalMovement.forward + cosine * originalMovement.side;

        return runtime::Result<ResolvedCommand>::success({ draft, shotRequested });
    }

    runtime::Result<ResolvedCommand> CommandPipeline::run(std::vector<CommandIntent> intents) noexcept
    {
        auto captured = commandRuntime_.capture();
        if (!captured)
            return runtime::Result<ResolvedCommand>::failure(captured.error());

        PredictionScope prediction{ predictionRuntime_, captured.value() };
        if (!prediction.active() && prediction.status().error().code != runtime::StatusCode::Unsupported)
            return runtime::Result<ResolvedCommand>::failure(prediction.status().error());

        auto resolved = resolver_.resolve(captured.value(), std::move(intents));
        if (!resolved)
            return resolved;
        const auto committed = commandRuntime_.commit(resolved.value());
        if (!committed)
            return runtime::Result<ResolvedCommand>::failure(committed.error());
        return resolved;
    }
}
