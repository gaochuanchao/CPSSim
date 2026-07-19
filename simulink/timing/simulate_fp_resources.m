function [jobs, schedule] = simulate_fp_resources( ...
        cfg, tasks, jobs, stopTick)
%SIMULATE_FP_RESOURCES Simulate preemptive fixed-priority resources.
%
%   [jobs, schedule] = simulate_fp_resources( ...
%       cfg, tasks, jobs, stopTick)
%
%   Each distinct resource name represents one uniprocessor.
%
%   Scheduling policy:
%       - fixed priorities;
%       - smaller numeric value means higher priority;
%       - fully preemptive;
%       - equal-priority jobs do not preempt the running job;
%       - ready-queue tie breaking uses:
%             priority,
%             release time,
%             task ID,
%             job ID.
%
%   Tick semantics:
%       - releases occur at the beginning of a tick;
%       - dispatch/preemption occurs at the beginning of a tick;
%       - execution occurs during [tick, tick + 1);
%       - a job consuming its final execution tick completes at
%         the beginning of the next tick.
%
%   The scheduler records a first-start event separately from a
%   resume event. Only first-start events will later be translated
%   into FMU activation triggers.

    %% Input validation

    if ~isstruct(cfg) || ~isfield(cfg, "sampleTimeSec")
        error("cfg.sampleTimeSec is required.");
    end

    if ~istable(tasks)
        error("tasks must be a MATLAB table.");
    end

    if ~istable(jobs)
        error("jobs must be a MATLAB table.");
    end

    validateIntegerScalar(stopTick, "stopTick");

    if stopTick < 0
        error("stopTick must be nonnegative.");
    end

    stopTick = double(stopTick);

    requiredJobColumns = [
        "taskId"
        "taskName"
        "jobId"
        "resource"
        "priority"
        "releaseTick"
        "absoluteDeadlineTick"
        "executionTicks"
        "remainingTicks"
        "state"
        "hasStarted"
        "firstStartTick"
        "finishTick"
        "preemptionCount"
        "deadlineMiss"
    ];

    availableColumns = string(jobs.Properties.VariableNames);

    missingColumns = requiredJobColumns( ...
        ~ismember(requiredJobColumns, availableColumns));

    if ~isempty(missingColumns)
        error( ...
            "The job table is missing columns: %s.", ...
            strjoin(missingColumns, ", "));
    end

    if any(jobs.state ~= "notReleased")
        error("All jobs must initially have state 'notReleased'.");
    end

    if any(jobs.remainingTicks ~= jobs.executionTicks)
        error("Every job must initially have its full execution budget.");
    end

    %% Resource information

    resourceNames = unique(string(tasks.resource), "stable");
    numResources = numel(resourceNames);

    [resourceFound, jobResourceIndex] = ...
        ismember(string(jobs.resource), resourceNames);

    if ~all(resourceFound)
        error("At least one job refers to an undefined resource.");
    end

    %% Runtime resource state

    % The value is the row number of the running job.
    % Zero means that the resource is idle.
    runningJob = zeros(numResources, 1);

    % Each cell contains row numbers of ready jobs.
    readyJobs = cell(numResources, 1);
    for resourceIndex = 1:numResources
        readyJobs{resourceIndex} = [];
    end

    %% Detect unsupported self-overlapping jobs

    % The current FMU abstraction has one private computation state
    % per task. We therefore prohibit two simultaneously active jobs
    % of the same task in this initial scheduler.
    maxTaskId = max(tasks.taskId);
    activeJobByTask = zeros(maxTaskId, 1);

    %% Execution timeline

    % Row k+1 represents interval:
    %
    %     [k, k+1)
    %
    % Each entry stores the corresponding row in the jobs table.
    % Zero denotes an idle processor.

    executionJobRow = zeros( ...
        stopTick, numResources, "uint32");

    %% Efficient release and deadline processing

    % generate_periodic_jobs sorts jobs by release time.
    nextReleaseIndex = 1;

    [~, deadlineOrder] = sort( ...
        jobs.absoluteDeadlineTick, "ascend");

    nextDeadlineIndex = 1;

    numJobs = height(jobs);

    %% Event-log storage

    eventCapacity = max(1024, 6 * numJobs);
    numEvents = 0;

    eventTick = zeros(eventCapacity, 1);
    eventType = strings(eventCapacity, 1);
    eventTaskId = zeros(eventCapacity, 1);
    eventTaskName = strings(eventCapacity, 1);
    eventJobId = zeros(eventCapacity, 1);
    eventJobRow = zeros(eventCapacity, 1);
    eventResource = strings(eventCapacity, 1);

    %% Main scheduler loop

    for tick = 0:stopTick

        %--------------------------------------------------------------
        % 1. Complete jobs whose last execution interval ended now.
        %--------------------------------------------------------------

        for resourceIndex = 1:numResources
            currentJob = runningJob(resourceIndex);

            if currentJob == 0
                continue;
            end

            if jobs.remainingTicks(currentJob) == 0
                jobs.state(currentJob) = "completed";
                jobs.finishTick(currentJob) = tick;

                if tick > jobs.absoluteDeadlineTick(currentJob)
                    jobs.deadlineMiss(currentJob) = true;
                end

                appendEvent(tick, "finish", currentJob);

                taskId = jobs.taskId(currentJob);

                if activeJobByTask(taskId) ~= currentJob
                    error( ...
                        "Internal active-job tracking error for task %d.", ...
                        taskId);
                end

                activeJobByTask(taskId) = 0;
                runningJob(resourceIndex) = 0;
            end
        end

        %--------------------------------------------------------------
        % 2. Detect deadline misses at this instant.
        %
        % A completion exactly at the absolute deadline is on time,
        % because completions were processed first.
        %--------------------------------------------------------------

        while nextDeadlineIndex <= numJobs
            jobRow = deadlineOrder(nextDeadlineIndex);
            deadline = jobs.absoluteDeadlineTick(jobRow);

            if deadline > tick
                break;
            end

            if deadline == tick && ...
                    jobs.state(jobRow) ~= "completed"

                jobs.deadlineMiss(jobRow) = true;
                appendEvent(tick, "deadlineMiss", jobRow);
            end

            nextDeadlineIndex = nextDeadlineIndex + 1;
        end

        %--------------------------------------------------------------
        % 3. Release all jobs assigned to this tick.
        %--------------------------------------------------------------

        while nextReleaseIndex <= numJobs && ...
                jobs.releaseTick(nextReleaseIndex) == tick

            jobRow = nextReleaseIndex;
            taskId = jobs.taskId(jobRow);

            previousJob = activeJobByTask(taskId);

            if previousJob ~= 0
                error( ...
                    [ ...
                    "Unsupported self-overlap for task '%s'. " ...
                    "Job %d is released at tick %d while job %d " ...
                    "is still active. An explicit overrun policy " ...
                    "is required." ...
                    ], ...
                    jobs.taskName(jobRow), ...
                    jobs.jobId(jobRow), ...
                    tick, ...
                    jobs.jobId(previousJob));
            end

            jobs.state(jobRow) = "ready";
            activeJobByTask(taskId) = jobRow;

            resourceIndex = jobResourceIndex(jobRow);

            readyJobs{resourceIndex} = enqueueReadyJob( ...
                readyJobs{resourceIndex}, ...
                jobRow, ...
                numJobs);

            appendEvent(tick, "release", jobRow);

            nextReleaseIndex = nextReleaseIndex + 1;
        end

        %--------------------------------------------------------------
        % 4. Dispatch or preempt on each resource.
        %--------------------------------------------------------------
        
        for resourceIndex = 1:numResources
            queue = readyJobs{resourceIndex};
        
            validateReadyQueue(queue, numJobs, resourceIndex);
        
            if isempty(queue)
                continue;
            end
        
            [bestReadyJob, bestQueuePosition] = ...
                selectBestReadyJob(queue, jobs);
        
            currentJob = runningJob(resourceIndex);
        
            if currentJob == 0
                %------------------------------------------------------
                % Idle processor: dispatch the best ready job.
                %------------------------------------------------------
        
                queue(bestQueuePosition) = [];
                readyJobs{resourceIndex} = queue(:);
        
                runningJob(resourceIndex) = bestReadyJob;
                jobs.state(bestReadyJob) = "running";
        
                if ~jobs.hasStarted(bestReadyJob)
                    jobs.hasStarted(bestReadyJob) = true;
                    jobs.firstStartTick(bestReadyJob) = tick;
        
                    appendEvent(tick, "start", bestReadyJob);
                else
                    appendEvent(tick, "resume", bestReadyJob);
                end
        
            elseif jobs.priority(bestReadyJob) < ...
                    jobs.priority(currentJob)
        
                %------------------------------------------------------
                % A strictly higher-priority job preempts the current job.
                %------------------------------------------------------
        
                assert(currentJob >= 1 && currentJob <= numJobs, ...
                    "Running job index is invalid.");
        
                jobs.state(currentJob) = "ready";
        
                jobs.preemptionCount(currentJob) = ...
                    jobs.preemptionCount(currentJob) + 1;
        
                appendEvent(tick, "preempt", currentJob);
        
                % Remove the newly selected job from the ready queue.
                queue(bestQueuePosition) = [];
        
                % Return the preempted job to the ready queue.
                queue = enqueueReadyJob( ...
                    queue, ...
                    currentJob, ...
                    numJobs);
        
                readyJobs{resourceIndex} = queue;
        
                % Dispatch the higher-priority job.
                runningJob(resourceIndex) = bestReadyJob;
                jobs.state(bestReadyJob) = "running";
        
                if ~jobs.hasStarted(bestReadyJob)
                    jobs.hasStarted(bestReadyJob) = true;
                    jobs.firstStartTick(bestReadyJob) = tick;
        
                    appendEvent(tick, "start", bestReadyJob);
                else
                    appendEvent(tick, "resume", bestReadyJob);
                end
            end
        end

        %--------------------------------------------------------------
        % 5. Execute one scheduler tick.
        %
        % There is no execution interval after stopTick, but releases
        % and dispatches at stopTick are still logged for compatibility
        % with traces containing a sample exactly at the stop time.
        %--------------------------------------------------------------

        if tick < stopTick
            for resourceIndex = 1:numResources
                currentJob = runningJob(resourceIndex);

                if currentJob == 0
                    continue;
                end

                if jobs.remainingTicks(currentJob) <= 0
                    error( ...
                        "Job selected with no remaining execution time.");
                end

                executionJobRow(tick + 1, resourceIndex) = ...
                    uint32(currentJob);

                jobs.remainingTicks(currentJob) = ...
                    jobs.remainingTicks(currentJob) - 1;
            end
        end
    end

    %% Finalize event table

    eventTick = eventTick(1:numEvents);
    eventType = eventType(1:numEvents);
    eventTaskId = eventTaskId(1:numEvents);
    eventTaskName = eventTaskName(1:numEvents);
    eventJobId = eventJobId(1:numEvents);
    eventJobRow = eventJobRow(1:numEvents);
    eventResource = eventResource(1:numEvents);

    sequence = (1:numEvents)';
    eventTimeSec = eventTick .* cfg.sampleTimeSec;

    eventLog = table( ...
        sequence, ...
        eventTimeSec, ...
        eventTick, ...
        eventType, ...
        eventTaskId, ...
        eventTaskName, ...
        eventJobId, ...
        eventJobRow, ...
        eventResource);

    %% Resource utilization

    if stopTick > 0
        busyTicks = sum(executionJobRow ~= 0, 1)';
        utilization = busyTicks ./ stopTick;
    else
        busyTicks = zeros(numResources, 1);
        utilization = nan(numResources, 1);
    end

    resourceTable = table( ...
        resourceNames, ...
        busyTicks, ...
        utilization);

    %% Return scheduler results

    schedule = struct();

    schedule.stopTick = stopTick;
    schedule.sampleTimeSec = cfg.sampleTimeSec;
    schedule.resourceNames = resourceNames;
    schedule.executionJobRow = executionJobRow;

    schedule.eventLog = eventLog;

    schedule.releaseEvents = ...
        eventLog(eventLog.eventType == "release", :);

    schedule.startEvents = ...
        eventLog(eventLog.eventType == "start", :);

    schedule.resumeEvents = ...
        eventLog(eventLog.eventType == "resume", :);

    schedule.preemptionEvents = ...
        eventLog(eventLog.eventType == "preempt", :);

    schedule.finishEvents = ...
        eventLog(eventLog.eventType == "finish", :);

    schedule.deadlineMissEvents = ...
        eventLog(eventLog.eventType == "deadlineMiss", :);

    schedule.resourceTable = resourceTable;

    %% Nested event-logging function

    function appendEvent(tickValue, typeValue, jobRowValue)
        numEvents = numEvents + 1;

        if numEvents > eventCapacity
            newCapacity = 2 * eventCapacity;

            eventTick(newCapacity, 1) = 0;
            eventType(newCapacity, 1) = "";
            eventTaskId(newCapacity, 1) = 0;
            eventTaskName(newCapacity, 1) = "";
            eventJobId(newCapacity, 1) = 0;
            eventJobRow(newCapacity, 1) = 0;
            eventResource(newCapacity, 1) = "";

            eventCapacity = newCapacity;
        end

        eventTick(numEvents) = tickValue;
        eventType(numEvents) = typeValue;

        eventTaskId(numEvents) = jobs.taskId(jobRowValue);
        eventTaskName(numEvents) = jobs.taskName(jobRowValue);
        eventJobId(numEvents) = jobs.jobId(jobRowValue);
        eventJobRow(numEvents) = jobRowValue;
        eventResource(numEvents) = jobs.resource(jobRowValue);
    end
end


function [bestJob, bestPosition] = selectBestReadyJob(queue, jobs)
%SELECTBESTREADYJOB Apply deterministic fixed-priority ordering.

    queue = double(queue(:));

    validateReadyQueue(queue, height(jobs), NaN);

    if isempty(queue)
        error("selectBestReadyJob received an empty ready queue.");
    end

    keys = [
        jobs.priority(queue), ...
        jobs.releaseTick(queue), ...
        jobs.taskId(queue), ...
        jobs.jobId(queue)
    ];

    [~, order] = sortrows(keys, [1, 2, 3, 4]);

    bestPosition = order(1);
    bestJob = queue(bestPosition);
end


function queue = enqueueReadyJob(queue, jobRow, numJobs)
%ENQUEUEREADYJOB Append a valid job-table row to a ready queue.

    queue = double(queue(:));
    jobRow = double(jobRow);
    
    if ~isscalar(jobRow) || ...
            ~isfinite(jobRow) || ...
            jobRow < 1 || ...
            jobRow > numJobs || ...
            jobRow ~= fix(jobRow)
    
        error( ...
            "Cannot enqueue invalid job index %s. Valid range is 1:%d.", ...
            mat2str(jobRow), ...
            numJobs);
    end
    
    validateReadyQueue(queue, numJobs, NaN);
    
    queue = [queue; jobRow];
end


function validateReadyQueue(queue, numJobs, resourceIndex)
%VALIDATEREADYQUEUE Ensure that a queue contains valid job-table rows.

    if isempty(queue)
        return;
    end
    
    queue = double(queue(:));
    
    invalidEntry = ...
        ~isfinite(queue) | ...
        queue < 1 | ...
        queue > numJobs | ...
        queue ~= fix(queue);
    
    if any(invalidEntry)
        if isnan(resourceIndex)
            resourceText = "";
        else
            resourceText = sprintf(" for resource %d", resourceIndex);
        end
    
        error( ...
            "Invalid ready queue%s: %s. Valid job rows are 1:%d.", ...
            resourceText, ...
            mat2str(queue'), ...
            numJobs);
    end
    
    if numel(unique(queue)) ~= numel(queue)
        error( ...
            "A ready queue contains duplicate job rows: %s.", ...
            mat2str(queue'));
    end
end


function validateIntegerScalar(value, parameterName)
%VALIDATEINTEGERSCALAR Validate a scheduler tick parameter.

    if ~isnumeric(value) || ...
            ~isscalar(value) || ...
            ~isfinite(value) || ...
            abs(value - round(value)) > 1e-12

        error("%s must be a finite integer scalar.", parameterName);
    end
end