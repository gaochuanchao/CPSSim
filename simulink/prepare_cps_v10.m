%% Prepare the CPS Challenge single-vehicle example
%
% This script:
%   1. Loads the example_v_10 trajectory.
%   2. Generates periodic task activation/completion events.
%   3. Generates fixed-delay network events.
%   4. Creates two timeseries objects for Simulink.

clearvars;

%% Repository and user configuration

% Derive the repository root from this script's location so the workflow does
% not depend on the repository folder name or one developer's machine.
scriptPath = mfilename("fullpath");
repoRoot = fileparts(fileparts(scriptPath));

dataDir = fullfile(repoRoot, "examples", "example_v_10");

Ts = 1e-4;       % 0.1 ms FMU and Simulink step
Tstop = 15.0;    % Run 15 seconds first

%% Load the supplied trajectory

timeAll = readmatrix(fullfile(dataDir, "time_vector.csv"));
ff0All  = readmatrix(fullfile(dataDir, ...
    "feedforward_sequence_0.csv"));
ff1All  = readmatrix(fullfile(dataDir, ...
    "feedforward_sequence_1.csv"));
velAll  = readmatrix(fullfile(dataDir, "velocity.csv"));

% Convert all data to column vectors
timeAll = timeAll(:);
ff0All  = ff0All(:);
ff1All  = ff1All(:);
velAll  = velAll(:);

% Protect against inconsistent file lengths
n = min([numel(timeAll), numel(ff0All), ...
         numel(ff1All), numel(velAll)]);

rawData = [timeAll(1:n), ff0All(1:n), ...
           ff1All(1:n), velAll(1:n)];

% Remove rows containing NaN or Inf
rawData = rawData(all(isfinite(rawData), 2), :);

timeAll = rawData(:, 1);
ff0All  = rawData(:, 2);
ff1All  = rawData(:, 3);
velAll  = rawData(:, 4);

%% Validate the supplied time base

if isempty(timeAll)
    error("The trajectory files contain no valid samples.");
end

if abs(timeAll(1)) > 1e-12
    error("The supplied time vector does not begin at zero.");
end

nCheck = min(numel(timeAll), 10001);
observedTs = median(diff(timeAll(1:nCheck)));

if abs(observedTs - Ts) > 1e-10
    error("Unexpected trace step: %.12g seconds.", observedTs);
end

%% Keep only the first Tstop seconds

keep = timeAll <= Tstop + Ts/2;

t   = timeAll(keep);
ff0 = ff0All(keep);
ff1 = ff1All(keep);
vel = velAll(keep);

numSamples = numel(t);

fprintf("Loaded %d samples from %.4f to %.4f seconds.\n", ...
    numSamples, t(1), t(end));

%% Create the 16 Boolean FMU trigger inputs

trigger = false(numSamples, 16);

% Trigger column mapping:
%
%  1: sensor activated
%  2: sensor finished
%  3: sensor-to-cloud packet sent
%  4: sensor-to-cloud packet received
%  5: estimator activated
%  6: estimator finished
%  7: controller activated
%  8: controller finished
%  9: feedforward activated
% 10: feedforward finished
% 11: merger activated
% 12: merger finished
% 13: cloud-to-actuator packet sent
% 14: cloud-to-actuator packet received
% 15: actuator activated
% 16: actuator finished

%% Sensor task: T = 5 ms, C = 0.6 ms

sensorRelease = 0 : 0.005 : Tstop;
sensorFinish  = sensorRelease + 0.0006;

trigger = addEvents(trigger, sensorRelease, 1, Ts);
trigger = addEvents(trigger, sensorFinish,  2, Ts);

% Transmit one simulation tick after publishing the sensor result.
sensorSend = sensorFinish + Ts;

% Fixed network latency of 8 ms.
sensorReceive = sensorSend + 0.008;

trigger = addEvents(trigger, sensorSend,    3, Ts);
trigger = addEvents(trigger, sensorReceive, 4, Ts);

%% Estimator task: T = 10 ms, C = 0.9 ms

estimatorRelease = 0 : 0.010 : Tstop;
estimatorFinish  = estimatorRelease + 0.0009;

trigger = addEvents(trigger, estimatorRelease, 5, Ts);
trigger = addEvents(trigger, estimatorFinish,  6, Ts);

%% Feedback controller: T = 20 ms, C = 0.3 ms

controllerRelease = 0 : 0.020 : Tstop;
controllerFinish  = controllerRelease + 0.0003;

trigger = addEvents(trigger, controllerRelease, 7, Ts);
trigger = addEvents(trigger, controllerFinish,  8, Ts);

%% Feedforward task: T = 20 ms, C = 1.2 ms

feedforwardRelease = 0 : 0.020 : Tstop;
feedforwardFinish  = feedforwardRelease + 0.0012;

trigger = addEvents(trigger, feedforwardRelease, 9,  Ts);
trigger = addEvents(trigger, feedforwardFinish,  10, Ts);

%% Merger task: T = 20 ms, C = 0.3 ms

mergerRelease = 0 : 0.020 : Tstop;
mergerFinish  = mergerRelease + 0.0003;

trigger = addEvents(trigger, mergerRelease, 11, Ts);
trigger = addEvents(trigger, mergerFinish,  12, Ts);

% Send the merged command one simulation tick after publication.
actuatorCommandSend = mergerFinish + Ts;
actuatorCommandReceive = actuatorCommandSend + 0.008;

trigger = addEvents(trigger, actuatorCommandSend,    13, Ts);
trigger = addEvents(trigger, actuatorCommandReceive, 14, Ts);

%% Actuator task: T = 30 ms, C = 0.5 ms

actuatorRelease = 0 : 0.030 : Tstop;
actuatorFinish  = actuatorRelease + 0.0005;

trigger = addEvents(trigger, actuatorRelease, 15, Ts);
trigger = addEvents(trigger, actuatorFinish,  16, Ts);

%% Create the Simulink input objects

% Logical data should produce Boolean Simulink signals.
trigger_ts = timeseries(logical(trigger), t);
trigger_ts.Name = "CPS task and network triggers";

% Columns: ff_ref_0, ff_ref_1, velocity
plant_inputs_ts = timeseries([ff0, ff1, vel], t);
plant_inputs_ts.Name = "CPS trajectory inputs";

%% Basic checks

fprintf("Number of sensor activations:      %d\n", ...
    nnz(trigger(:, 1)));
fprintf("Number of estimator activations:   %d\n", ...
    nnz(trigger(:, 5)));
fprintf("Number of controller activations:  %d\n", ...
    nnz(trigger(:, 7)));
fprintf("Number of actuator activations:    %d\n", ...
    nnz(trigger(:, 15)));

fprintf("Initial velocity: %.3f m/s\n", vel(1));
fprintf("Prepared experiment stop time: %.3f s\n", Tstop);

%% Optional: save the generated input data

save("cps_v10_inputs.mat", ...
    "trigger_ts", "plant_inputs_ts", ...
    "Ts", "Tstop", "-v7.3");

%% Local helper function

function signalMatrix = addEvents( ...
        signalMatrix, eventTimes, column, sampleTime)

    indices = round(eventTimes(:) ./ sampleTime) + 1;

    valid = indices >= 1 & indices <= size(signalMatrix, 1);
    indices = indices(valid);

    signalMatrix(indices, column) = true;
end
