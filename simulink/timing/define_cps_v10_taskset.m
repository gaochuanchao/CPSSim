function [cfg, tasks] = define_cps_v10_taskset(mappingMode)
%DEFINE_CPS_V10_TASKSET Define the single-vehicle CPS task set.
%
%   [cfg, tasks] = define_cps_v10_taskset()
%   creates the task set with one dedicated virtual processor per task.
%
%   [cfg, tasks] = define_cps_v10_taskset("dedicated")
%   is equivalent to the default mode.
%
%   [cfg, tasks] = define_cps_v10_taskset("cloud1")
%   maps all four cloud tasks onto one shared cloud processor.
%
%   Timing values are represented as integer scheduler ticks.
%   One tick corresponds to 0.1 ms.

    if nargin < 1
        mappingMode = "dedicated";
    end

    mappingMode = string(mappingMode);

    validModes = ["dedicated", "cloud1"];

    if ~ismember(mappingMode, validModes)
        error( ...
            "Unknown mapping mode '%s'. Valid modes are: %s.", ...
            mappingMode, strjoin(validModes, ", "));
    end

    %% Global timing configuration

    cfg = struct();

    cfg.sampleTimeSec = 1e-4;          % 0.1 ms
    cfg.sendOffsetTicks = 1;           % Send one tick after publication
    cfg.networkDelayTicks = 80;        % 8 ms / 0.1 ms
    cfg.numTriggerColumns = 16;
    cfg.mappingMode = mappingMode;
    % Network-event trigger columns in the FMU input vector.
    cfg.uplinkSendColumn = 3;
    cfg.uplinkReceiveColumn = 4;
    cfg.downlinkSendColumn = 13;
    cfg.downlinkReceiveColumn = 14;

    %% Task names and functional locations

    name = [
        "Sensor"
        "Estimator"
        "Controller"
        "Feedforward"
        "Merger"
        "Actuator"
    ];

    platform = [
        "vehicle"
        "cloud"
        "cloud"
        "cloud"
        "cloud"
        "vehicle"
    ];

    %% Timing parameters in seconds

    periodSec = [
        0.0050     % Sensor:       5 ms
        0.0100     % Estimator:   10 ms
        0.0200     % Controller:  20 ms
        0.0200     % Feedforward: 20 ms
        0.0200     % Merger:      20 ms
        0.0300     % Actuator:    30 ms
    ];

    executionSec = [
        0.0006     % Sensor:       0.6 ms
        0.0009     % Estimator:    0.9 ms
        0.0003     % Controller:   0.3 ms
        0.0012     % Feedforward:  1.2 ms
        0.0003     % Merger:       0.3 ms
        0.0005     % Actuator:     0.5 ms
    ];

    % Initial baseline: implicit deadlines.
    deadlineSec = periodSec;

    % Initial baseline: synchronous releases at time zero.
    offsetSec = zeros(size(periodSec));

    %% Fixed-priority assignment

    % Smaller value means higher priority.
    %
    % Priorities are only compared among tasks sharing a resource.
    % For the cloud:
    %
    % Estimator > Controller > Feedforward > Merger

    priority = [
        1     % Sensor
        1     % Estimator
        2     % Controller
        3     % Feedforward
        4     % Merger
        1     % Actuator
    ];

    %% FMU trigger-column mapping

    activationColumn = [
         1     % Sensor activated
         5     % Estimator activated
         7     % Controller activated
         9     % Feedforward activated
        11     % Merger activated
        15     % Actuator activated
    ];

    finishColumn = [
         2     % Sensor finished
         6     % Estimator finished
         8     % Controller finished
        10     % Feedforward finished
        12     % Merger finished
        16     % Actuator finished
    ];

    %% Resource mapping

    switch mappingMode
        case "dedicated"
            resource = [
                "vehicle_sensor_cpu"
                "cloud_estimator_cpu"
                "cloud_controller_cpu"
                "cloud_feedforward_cpu"
                "cloud_merger_cpu"
                "vehicle_actuator_cpu"
            ];

        case "cloud1"
            resource = [
                "vehicle_sensor_cpu"
                "cloud_cpu_1"
                "cloud_cpu_1"
                "cloud_cpu_1"
                "cloud_cpu_1"
                "vehicle_actuator_cpu"
            ];
    end

    %% Convert all timing parameters to integer ticks

    periodTicks = secondsToTicks( ...
        periodSec, cfg.sampleTimeSec, "period");

    executionTicks = secondsToTicks( ...
        executionSec, cfg.sampleTimeSec, "execution time");

    deadlineTicks = secondsToTicks( ...
        deadlineSec, cfg.sampleTimeSec, "deadline");

    offsetTicks = secondsToTicks( ...
        offsetSec, cfg.sampleTimeSec, "offset");

    %% Construct task table

    taskId = (1:numel(name))';

    tasks = table( ...
        taskId, ...
        name, ...
        platform, ...
        resource, ...
        periodTicks, ...
        executionTicks, ...
        deadlineTicks, ...
        offsetTicks, ...
        priority, ...
        activationColumn, ...
        finishColumn);

    %% Sanity checks

    assert(all(tasks.periodTicks > 0), ...
        "Every task must have a positive period.");

    assert(all(tasks.executionTicks > 0), ...
        "Every task must have a positive execution time.");

    assert(all(tasks.deadlineTicks > 0), ...
        "Every task must have a positive deadline.");

    assert(all(tasks.executionTicks <= tasks.deadlineTicks), ...
        "Execution time must not exceed the relative deadline.");

    usedColumns = [
        tasks.activationColumn
        tasks.finishColumn
    ];

    assert(numel(unique(usedColumns)) == numel(usedColumns), ...
        "Task trigger columns must be unique.");

    assert(all(usedColumns >= 1 & ...
               usedColumns <= cfg.numTriggerColumns), ...
        "A task trigger column is outside the valid range.");
end


function ticks = secondsToTicks(valueSec, sampleTimeSec, parameterName)
%SECONDSTOTICKS Convert representable times into integer ticks.

    ticks = round(valueSec ./ sampleTimeSec);

    reconstructedSec = ticks .* sampleTimeSec;
    tolerance = max(1e-12, sampleTimeSec * 1e-9);

    if any(abs(reconstructedSec - valueSec) > tolerance)
        error( ...
            "A %s value is not an integer multiple of %.12g seconds.", ...
            parameterName, sampleTimeSec);
    end

    ticks = double(ticks);
end