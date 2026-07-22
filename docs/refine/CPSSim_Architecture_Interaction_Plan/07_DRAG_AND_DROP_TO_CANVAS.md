# Step 7 — Drag a Task from Component Library to the Architecture Canvas

## Objective

Support the later DesCartes-style workflow:

```text
drag Task from component library
    ↓
drop at a canvas position
    ↓
create task through shared controller
    ↓
place task at dropped scene position
```

The drop must not create a QtNodes-only object.

## 1. MIME contract

Define one application-specific MIME type:

```text
application/x-cpssim-component
```

Payload values should be stable strings:

```text
task
resource
connection
```

For the initial canvas drop implementation, Architecture accepts only:

```text
task
```

Do not accept arbitrary text labels such as `"Task"` because display text may change.

## 2. Component library drag source

A practical implementation may use:

- `QListWidget` with custom MIME data; or
- a small custom `QToolButton`/widget that initiates `QDrag`.

Prefer the simplest implementation consistent with the current library UI.

Required drag data:

```cpp
auto* mime = new QMimeData;
mime->setData(
    "application/x-cpssim-component",
    QByteArrayLiteral("task"));
```

Use `Qt::CopyAction`.

Do not remove the component from the library after a drag.

## 3. Architecture graphics view drop support

Extend `QtArchitectureGraphicsView`:

```cpp
setAcceptDrops(true);
viewport()->setAcceptDrops(true);
```

Depending on QGraphicsView event delivery, verify whether drop events arrive on the view or viewport in the installed Qt version.

Override:

```cpp
void dragEnterEvent(QDragEnterEvent*) override;
void dragMoveEvent(QDragMoveEvent*) override;
void dropEvent(QDropEvent*) override;
```

Use callbacks to `QtArchitectureView`, similar to context-menu forwarding.

Recommended callback:

```cpp
using ComponentDropHandler =
    std::function<bool(
        QByteArrayView component_type,
        QPointF scene_position)>;
```

Do not give the private graphics-view subclass a bridge or controller reference.

## 4. Drag acceptance rules

Accept only when:

- MIME type matches;
- payload is exactly `task`;
- editable system exists;
- simulation is not running;
- project policy allows task creation.

During drag move:

- show Copy cursor for valid positions;
- reject otherwise.

Do not create the task during drag enter or drag move.

Create only in `dropEvent(...)`.

## 5. Coordinate conversion

Convert the drop position to scene coordinates using the actual view transform:

```cpp
const QPointF scene_position =
    mapToScene(event->position().toPoint());
```

Use the appropriate Qt 6 event API available in the current build.

Do not use raw viewport coordinates as workspace positions.

## 6. Drop handling

The Architecture view handler should call:

```cpp
add_task_at(scene_position);
```

That method must already use the shared controller after Step 2.

On success:

- accept proposed action;
- select the task;
- persist snapped/non-overlapping position;
- refresh through normal signals.

On failure:

- ignore drop;
- preserve current selection;
- show status only when a useful diagnostic exists.

## 7. Position behavior

The requested drop position is the preferred top-left or center according to the existing node-placement convention.

Use the same convention consistently as:

```text
add_task_at(...)
next_available_node_position(...)
snap_architecture_position(...)
```

Do not add a second collision-avoidance algorithm.

If the dropped location overlaps an existing node:

- offset by the existing grid step;
- find the next available location;
- keep within scene without arbitrary viewport clamping.

## 8. Resource and connection drops

In this step:

```text
resource drop on Architecture → reject
connection drop on Architecture → reject
```

Resource assignment remains managed through forms/tables.

Connection creation remains a port gesture.

A later design may support dropping a resource onto a task to assign it, but that is explicitly outside this step.

## 9. Drag visual

Use native Qt drag feedback first.

Optional:

- a small task icon or pixmap;
- tooltip/status text.

Do not implement a live temporary QtNodes node that follows the cursor.

## 10. Tests

### MIME generation test

- Task library item produces correct MIME type and payload.
- Drag action is Copy.

### Drop handler test

Test the handler directly without synthesizing complex OS drag events where possible:

- valid `task` payload calls `add_task_at`;
- invalid payload rejected;
- resource rejected;
- running-state rejected;
- protected-project rejected.

### Coordinate test

- drop at known viewport coordinate under a non-identity zoom/pan transform;
- verify resulting workspace scene position matches `mapToScene`;
- verify snapped position.

### Integration test

- count tasks;
- invoke drop handler at scene point;
- verify count +1;
- verify model node +1;
- verify selected task;
- undo and redo.

## 11. Manual acceptance

1. Zoom Architecture to 150%.
2. Pan away from origin.
3. Drag Task from library.
4. Drop at visible position.
5. Confirm node appears under/near cursor in scene coordinates.
6. Drop on occupied node.
7. Confirm collision avoidance.
8. Undo and redo.
9. Drag Resource to canvas.
10. Confirm rejection without side effects.
11. Start simulation and attempt drag.
12. Confirm rejection.
13. Test dark/light themes and different DPI monitor.

## 12. Prohibited changes

Copilot must not:

- create task during dragMove;
- create a temporary domain task for preview;
- use screen/global coordinates for node position;
- accept plain text drops;
- place resource nodes;
- bypass controller/undo;
- create scene-only nodes;
- remove click-to-add behavior from the component library.
