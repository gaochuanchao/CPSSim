%% Compare real and remote-platform performance monitors
%
% Run after the Simulink simulation has completed.

if ~exist("out", "var")
    error("Simulation output variable 'out' was not found. " + ...
        "Run the Simulink model first." ...
        );
end

if ~isa(out, "Simulink.SimulationOutput")
    error("'out' is not a Simulink.SimulationOutput object.");
end

availableOutputs = string(who(out));
requiredOutputs = ["rolling_real", "rolling_remote"];

missingOutputs = requiredOutputs( ...
    ~ismember(requiredOutputs, availableOutputs));

if ~isempty(missingOutputs)
    error( ...
        "The following output was not found inside 'out': %s", ...
        strjoin(missingOutputs, ", ") ...
        );
end

rollingReal = out.rolling_real;
rollingRemote = out.rolling_remote;

if ~isa(rollingReal, "timeseries")
    error("'out.rolling_real' is not a timeseries object.");
end

if ~isa(rollingRemote, "timeseries")
    error("'out.rolling_remote' is not a timeseries object.");
end

tReal = rollingReal.Time;
yReal = squeeze(rollingReal.Data);

tRemote = rollingRemote.Time;
yRemote = squeeze(rollingRemote.Data);

figure("Name", "Performance Monitor Comparison");

plot(tReal, yReal, "LineWidth", 1.1);
hold on;

plot(tRemote, yRemote, "LineWidth", 1.1);

xlabel("Time (s)");
ylabel("Rolling performance metric");
title("Real and Remote-Platform Performance Monitors");

legend( ...
    "Ground-truth performance", ...
    "Remote-platform estimate", ...
    "Location", "best" ...
    );

grid on;
box on;