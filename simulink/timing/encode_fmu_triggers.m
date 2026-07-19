function [trigger, networkLog] = encode_fmu_triggers( ...
        cfg, tasks, jobs, stopTick)
%ENCODE_FMU_TRIGGERS Convert scheduler results to FMU trigger signals.
%
%   [trigger, networkLog] = encode_fmu_triggers( ...
%       cfg, tasks, jobs, stopTick)
%
%   Output:
%       trigger
%           Logical matrix of size (stopTick + 1)-by-16.
%           Row tick+1 represents the event values at that tick.
%
%       networkLog
%           Table describing causally generated packet-send and
%           packet-receive events.
%
%   Mapping:
%       first dispatch -> task activation trigger
%       completion     -> task finish trigger
%
%   A resumed job does not generate another activation trigger.

    %% Validate inputs

    if ~isstruct(cfg)
        error("cfg must be a structure.");
    end

    requiredCfgFields = [
        "numTriggerColumns"
        "sendOffsetTicks"
        "networkDelayTicks"
        "uplinkSendColumn"
        "uplinkReceiveColumn"
        "downlinkSendColumn"
        "downlinkReceiveColumn"
    ];

    for fieldIndex = 1:numel(requiredCfgFields)
        fieldName = requiredCfgFields(fieldIndex);

        if ~isfield(cfg, fieldName)
            error("cfg.%s is required.", fieldName);
        end
    end

    if ~istable(tasks)
        error("tasks must be a table.");
    end

    if ~istable(jobs)
        error("jobs must be a table.");
    end

    validateIntegerScalar(stopTick, "stopTick");

    if stopTick < 0
        error("stopTick must be nonnegative.");
    end

    stopTick = double(stopTick);

    requiredJobColumns = [
        "taskName"
        "jobId"
        "firstStartTick"
        "finishTick"
        "activationColumn"
        "finishColumn"
    ];

    availableColumns = string(jobs.Properties.VariableNames);

    missingColumns = requiredJobColumns( ...
        ~ismember(requiredJobColumns, availableColumns));

    if ~isempty(missingColumns)
        error( ...
            "The job table is missing columns: %s.", ...
            strjoin(missingColumns, ", "));
    end

    assert(nnz(tasks.name == "Sensor") == 1, ...
        "Exactly one Sensor task is required.");

    assert(nnz(tasks.name == "Merger") == 1, ...
        "Exactly one Merger task is required.");

    %% Allocate FMU trigger matrix

    numSamples = stopTick + 1;

    trigger = false( ...
        numSamples, ...
        cfg.numTriggerColumns);

    %% Encode task activation events

    validStart = ...
        isfinite(jobs.firstStartTick) & ...
        jobs.firstStartTick >= 0 & ...
        jobs.firstStartTick <= stopTick;

    trigger = addTickEvents( ...
        trigger, ...
        jobs.firstStartTick(validStart), ...
        jobs.activationColumn(validStart));

    %% Encode task completion events

    validFinish = ...
        isfinite(jobs.finishTick) & ...
        jobs.finishTick >= 0 & ...
        jobs.finishTick <= stopTick;

    trigger = addTickEvents( ...
        trigger, ...
        jobs.finishTick(validFinish), ...
        jobs.finishColumn(validFinish));

    %% Construct network messages from producer completions

    isNetworkProducer = ...
        isfinite(jobs.finishTick) & ...
        (jobs.taskName == "Sensor" | ...
         jobs.taskName == "Merger");

    producerJobs = jobs(isNetworkProducer, :);

    numPackets = height(producerJobs);

    packetId = (1:numPackets)';
    direction = strings(numPackets, 1);

    sourceTaskName = producerJobs.taskName;
    sourceJobId = producerJobs.jobId;

    publishTick = producerJobs.finishTick;
    sendTick = publishTick + cfg.sendOffsetTicks;
    receiveTick = sendTick + cfg.networkDelayTicks;

    sendColumn = zeros(numPackets, 1);
    receiveColumn = zeros(numPackets, 1);

    sensorPacket = sourceTaskName == "Sensor";
    mergerPacket = sourceTaskName == "Merger";

    direction(sensorPacket) = "uplink";
    direction(mergerPacket) = "downlink";

    sendColumn(sensorPacket) = cfg.uplinkSendColumn;
    receiveColumn(sensorPacket) = cfg.uplinkReceiveColumn;

    sendColumn(mergerPacket) = cfg.downlinkSendColumn;
    receiveColumn(mergerPacket) = cfg.downlinkReceiveColumn;

    sendInTrace = ...
        sendTick >= 0 & ...
        sendTick <= stopTick;

    receiveInTrace = ...
        receiveTick >= 0 & ...
        receiveTick <= stopTick;

    %% Encode packet-send and packet-receive events

    trigger = addTickEvents( ...
        trigger, ...
        sendTick(sendInTrace), ...
        sendColumn(sendInTrace));

    trigger = addTickEvents( ...
        trigger, ...
        receiveTick(receiveInTrace), ...
        receiveColumn(receiveInTrace));

    %% Produce a readable network log

    networkLog = table( ...
        packetId, ...
        direction, ...
        sourceTaskName, ...
        sourceJobId, ...
        publishTick, ...
        sendTick, ...
        receiveTick, ...
        sendColumn, ...
        receiveColumn, ...
        sendInTrace, ...
        receiveInTrace);

    networkLog = sortrows( ...
        networkLog, ...
        ["sendTick", "direction", "sourceJobId"]);

    %% Sanity checks

    assert(all(sendTick == ...
        publishTick + cfg.sendOffsetTicks));

    assert(all(receiveTick == ...
        sendTick + cfg.networkDelayTicks));

    assert(islogical(trigger));

    assert(size(trigger, 1) == stopTick + 1);
    assert(size(trigger, 2) == cfg.numTriggerColumns);
end


function signalMatrix = addTickEvents( ...
        signalMatrix, eventTicks, eventColumns)
%ADDTICKEVENTS Set Boolean events using zero-based scheduler ticks.

    if isempty(eventTicks)
        return;
    end

    eventTicks = double(eventTicks(:));
    eventColumns = double(eventColumns(:));

    if numel(eventTicks) ~= numel(eventColumns)
        error( ...
            "eventTicks and eventColumns must have equal length.");
    end

    invalidTick = ...
        ~isfinite(eventTicks) | ...
        eventTicks < 0 | ...
        eventTicks ~= fix(eventTicks) | ...
        eventTicks >= size(signalMatrix, 1);

    if any(invalidTick)
        error( ...
            "An event contains an invalid scheduler tick.");
    end

    invalidColumn = ...
        ~isfinite(eventColumns) | ...
        eventColumns < 1 | ...
        eventColumns ~= fix(eventColumns) | ...
        eventColumns > size(signalMatrix, 2);

    if any(invalidColumn)
        error( ...
            "An event contains an invalid trigger column.");
    end

    rowIndices = eventTicks + 1;

    linearIndices = sub2ind( ...
        size(signalMatrix), ...
        rowIndices, ...
        eventColumns);

    signalMatrix(linearIndices) = true;
end


function validateIntegerScalar(value, parameterName)
%VALIDATEINTEGERSCALAR Validate an integer-valued scalar.

    if ~isnumeric(value) || ...
            ~isscalar(value) || ...
            ~isfinite(value) || ...
            value ~= fix(value)

        error("%s must be a finite integer scalar.", parameterName);
    end
end