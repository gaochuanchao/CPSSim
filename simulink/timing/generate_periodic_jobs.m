function jobs = generate_periodic_jobs(tasks, stopTick)
%GENERATE_PERIODIC_JOBS Instantiate periodic jobs from a task table.
%
%   jobs = generate_periodic_jobs(tasks, stopTick)
%
%   Each task releases jobs at:
%
%       offsetTicks + k * periodTicks
%
%   for all release times not greater than stopTick.
%
%   This function only constructs the release calendar. It does not
%   perform scheduling, dispatch, execution, or completion.
%
%   All times are represented as integer scheduler ticks.

    %% Validate inputs

    if ~istable(tasks)
        error("tasks must be a MATLAB table.");
    end

    requiredColumns = [
        "taskId"
        "name"
        "resource"
        "periodTicks"
        "executionTicks"
        "deadlineTicks"
        "offsetTicks"
        "priority"
        "activationColumn"
        "finishColumn"
    ];

    availableColumns = string(tasks.Properties.VariableNames);

    missingColumns = requiredColumns( ...
        ~ismember(requiredColumns, availableColumns));

    if ~isempty(missingColumns)
        error( ...
            "The task table is missing columns: %s.", ...
            strjoin(missingColumns, ", "));
    end

    validateIntegerScalar(stopTick, "stopTick");

    if stopTick < 0
        error("stopTick must be nonnegative.");
    end

    stopTick = double(stopTick);

    %% Determine the total number of jobs

    numTasks = height(tasks);
    jobsPerTask = zeros(numTasks, 1);

    for taskIndex = 1:numTasks
        period = tasks.periodTicks(taskIndex);
        offset = tasks.offsetTicks(taskIndex);

        validateIntegerScalar(period, "periodTicks");
        validateIntegerScalar(offset, "offsetTicks");

        if period <= 0
            error("Task %s has a nonpositive period.", ...
                tasks.name(taskIndex));
        end

        if offset < 0
            error("Task %s has a negative release offset.", ...
                tasks.name(taskIndex));
        end

        if offset <= stopTick
            jobsPerTask(taskIndex) = ...
                floor((stopTick - offset) / period) + 1;
        end
    end

    totalJobs = sum(jobsPerTask);

    %% Preallocate job attributes

    taskId = zeros(totalJobs, 1);
    taskName = strings(totalJobs, 1);
    jobId = zeros(totalJobs, 1);

    resource = strings(totalJobs, 1);
    priority = zeros(totalJobs, 1);

    releaseTick = zeros(totalJobs, 1);
    absoluteDeadlineTick = zeros(totalJobs, 1);

    executionTicks = zeros(totalJobs, 1);
    remainingTicks = zeros(totalJobs, 1);

    activationColumn = zeros(totalJobs, 1);
    finishColumn = zeros(totalJobs, 1);

    %% Construct jobs task by task

    nextRow = 1;

    for taskIndex = 1:numTasks
        numJobs = jobsPerTask(taskIndex);

        if numJobs == 0
            continue;
        end

        rows = nextRow:(nextRow + numJobs - 1);

        releases = tasks.offsetTicks(taskIndex) + ...
            (0:(numJobs - 1))' * tasks.periodTicks(taskIndex);

        taskId(rows) = tasks.taskId(taskIndex);
        taskName(rows) = tasks.name(taskIndex);
        jobId(rows) = (1:numJobs)';

        resource(rows) = tasks.resource(taskIndex);
        priority(rows) = tasks.priority(taskIndex);

        releaseTick(rows) = releases;

        absoluteDeadlineTick(rows) = ...
            releases + tasks.deadlineTicks(taskIndex);

        executionTicks(rows) = tasks.executionTicks(taskIndex);
        remainingTicks(rows) = tasks.executionTicks(taskIndex);

        activationColumn(rows) = ...
            tasks.activationColumn(taskIndex);

        finishColumn(rows) = ...
            tasks.finishColumn(taskIndex);

        nextRow = nextRow + numJobs;
    end

    %% Runtime state fields

    % These fields will be modified by the scheduler in the next step.

    state = repmat("notReleased", totalJobs, 1);
    hasStarted = false(totalJobs, 1);

    firstStartTick = nan(totalJobs, 1);
    finishTick = nan(totalJobs, 1);

    preemptionCount = zeros(totalJobs, 1);
    deadlineMiss = false(totalJobs, 1);

    %% Construct and sort the job table

    jobs = table( ...
        taskId, ...
        taskName, ...
        jobId, ...
        resource, ...
        priority, ...
        releaseTick, ...
        absoluteDeadlineTick, ...
        executionTicks, ...
        remainingTicks, ...
        activationColumn, ...
        finishColumn, ...
        state, ...
        hasStarted, ...
        firstStartTick, ...
        finishTick, ...
        preemptionCount, ...
        deadlineMiss);

    % Deterministic ordering for jobs released simultaneously:
    % priority first, then task ID, then job ID.
    jobs = sortrows( ...
        jobs, ...
        ["releaseTick", "priority", "taskId", "jobId"]);
end


function validateIntegerScalar(value, parameterName)
%VALIDATEINTEGERSCALAR Check that a value is a finite integer scalar.

    if ~isnumeric(value) || ...
            ~isscalar(value) || ...
            ~isfinite(value) || ...
            abs(value - round(value)) > 1e-12

        error("%s must be a finite integer scalar.", parameterName);
    end
end