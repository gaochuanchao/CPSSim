# Step 4 — Redesign Task Ports for Interactive Connections

## Objective

Give every task node a stable connection interface:

```text
one input port
one output port
```

Both ports support multiple connections.

This step changes only the graph adapter/presentation. It does not yet create or delete domain routes through mouse gestures.

## 1. Current problem

The current rebuild logic derives task port counts from existing edges.

Consequences:

- an isolated task may have zero ports;
- the visual port index changes when edges change;
- a user cannot start a connection from a task with no existing outgoing edge;
- a user cannot finish a connection on a task with no existing incoming edge;
- ports represent existing edge ordinal positions rather than stable task endpoints.

This is suitable for read-only rendering but unsuitable for editing.

## 2. Target port contract

For every task node:

```cpp
input_count = 1;
output_count = 1;
```

Stable port indices:

```text
input port index:  0
output port index: 0
```

Connection policy:

```cpp
QtNodes::ConnectionPolicy::Many
```

All incoming graph edges connect to input port `0`.

All outgoing graph edges connect from output port `0`.

## 3. Change `QtArchitectureGraphModel::rebuild(...)`

When constructing each `NodeRecord`, initialize:

```cpp
.input_count = 1,
.output_count = 1,
```

Do not increment counts while iterating edges.

Remove logic equivalent to:

```cpp
std::map<NodeId, PortIndex> next_input;
std::map<NodeId, PortIndex> next_output;
const auto output_port = next_output[source]++;
const auto input_port = next_input[destination]++;
```

Replace each existing semantic edge's QtNodes connection ID with:

```cpp
QtNodes::ConnectionId{
    source_node_id,
    0,
    destination_node_id,
    0
}
```

## 4. Duplicate endpoint rule

With fixed port index `0`, the QtNodes `ConnectionId` is identical for two parallel edges between the same source and destination.

Therefore, the editing design must treat a second route with identical endpoints as a duplicate unless the domain model explicitly supports parallel routes with independent identities.

For the initial interactive editor:

```text
At most one displayed structural connection per ordered task pair.
```

Do not invent extra visual ports solely to encode parallel edges.

If existing project data can contain parallel routes, stop and add an explicit domain/visual design decision before implementing this step.

## 5. Keep semantic mapping

Continue to store the mapping:

```text
QtNodes::ConnectionId → GuiConnectionId
```

for existing displayed edges.

The mapping is required for:

- selecting a connection;
- showing connection properties;
- deleting a selected connection later.

Do not map by source/destination caption strings.

## 6. Update `portData(...)`

The model should return valid port data only when:

```cpp
index == 0
```

and the node exists.

Recommended checks:

```cpp
if (!nodes_.contains(node_id) || index != 0) {
    return {};
}
```

For both input and output:

```cpp
PortRole::DataType:
    "cpssim.connection"

PortRole::ConnectionPolicyRole:
    Many

PortRole::CaptionVisible:
    false

PortRole::Caption:
    empty
```

Do not assign a different type for each existing edge.

## 7. Preserve read-only connection mutation for this step

Keep:

```cpp
connectionPossible(...) == false
addConnection(...) does nothing
deleteConnection(...) == false
```

until Step 5.

The purpose of this step is to make stable ports visible and testable without mixing in domain mutation.

## 8. Node painter verification

Inspect:

```text
apps/qt_gui/architecture_node_painter.cpp
```

Confirm the custom painter delegates or correctly renders QtNodes ports.

Do not change node body dimensions unless ports are clipped.

Verify:

- input port is visible on left;
- output port is visible on right;
- resource accent stripe remains distinct;
- selected/invalid borders remain readable;
- captions and badges do not overlap ports.

Only make painter changes if visually necessary.

## 9. Connection rendering verification

Existing edges should remain attached to the correct tasks after the port change.

Test:

- one source to many destinations;
- many sources to one destination;
- a chain;
- a cycle;
- a self-loop if existing data supports it;
- no connections;
- invalid/unassigned tasks.

## 10. Tests

Update:

```text
tests/qt_gui/architecture_model_test.cpp
```

Required assertions:

### Isolated node

For an isolated task:

```cpp
nodeData(node, InPortCount) == 1
nodeData(node, OutPortCount) == 1
```

### Port data

For index `0`:

- data type is valid;
- policy is Many;
- caption hidden.

For index `1`:

- data is invalid/empty.

### Existing edge

Verify connection ID uses:

```text
outPortIndex == 0
inPortIndex == 0
```

### Fan-out

Two destinations from one source both use output port `0`.

### Fan-in

Two sources into one destination both use input port `0`.

### Rebuild stability

Rebuild with the same domain graph and confirm:

- node IDs remain mapped consistently;
- connection count remains correct;
- no duplicate record appears.

## 11. Manual acceptance

1. Open a project containing an isolated task.
2. Confirm one input and one output port are visible.
3. Open a project with several existing routes.
4. Confirm all routes render.
5. Move nodes and confirm connections follow.
6. Auto Layout and Fit All.
7. Toggle themes.
8. Confirm no connection is accidentally editable yet.

## 12. Prohibited changes

Copilot must not:

- create domain routes in this step;
- assign one port per edge;
- create dynamic port counts based on degree;
- add resource ports;
- show assignment edges as task connections;
- change connection semantics;
- add a second connection map;
- use caption strings as IDs.
