%% Plot the physical lateral error
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

availableOutputs = who(out);

if ~ismember("lateral_error", string(availableOutputs))
    error("'lateral_error' was not found inside 'out'. " + ...
        "Check the Variable name of the corresponding " + ...
        "To Workspace block." ...
    );
end

lateralError = out.lateral_error;

if ~isa(lateralError, "timeseries")
    error("'out.lateral_error' is not a timeseries object. " + ...
        "Set the To Workspace block Save format to Timeseries." ...
    );
end

t = lateralError.Time;
e = squeeze(lateralError.Data);

figure("Name", "Lateral Error");

plot(t, e, "LineWidth", 1.1);
hold on;

yline( 0.2, "--", "Comfort threshold: +0.2 m");
yline(-0.2, "--", "Comfort threshold: -0.2 m");

yline( 0.8, ":", "Safety threshold: +0.8 m");
yline(-0.8, ":", "Safety threshold: -0.8 m");

xlabel("Time (s)");
ylabel("Lateral error e_y (m)");
title("CPS Challenge: Physical Lateral Error");

grid on;
box on;
