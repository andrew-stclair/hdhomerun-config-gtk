# GEMINI instructions

When making changes, please try to compile the application using `ninja -C build` and then execute the application. you will be able to interact with the application using the `desktop_snapshot`, `desktop_find`, `desktop_click`, `desktop_type`, `desktop_key` and `desktop_capabilities`

## Available Tools

### `desktop_snapshot`

Capture the accessibility tree with semantic element references.

```
Parameters:
  app_name: str (optional) - Filter to specific application
  max_depth: int (default: 15) - Tree traversal depth

Returns:
  Tree of elements with ref_ids:
  - ref_1: [application] Firefox
    - ref_2: [frame] "GitHub - Mozilla Firefox"
      - ref_3: [button] "Back" (clickable)
      - ref_4: [entry] "Search or enter address" (editable, focused)
```

### `desktop_find`

Find elements by natural language query.

```
Parameters:
  query: str - "save button", "search field", "menu containing File"
  app_name: str (optional)

Returns:
  Matching elements with refs, states, and actions
```

### `desktop_click`

Click an element by reference or coordinates.

```
Parameters:
  ref: str - Element reference (e.g., "ref_5")
  element: str - Human description for logging
  coordinate: [x, y] - Fallback if no ref
  button: left|right|middle
  click_type: single|double
  modifiers: [ctrl, shift, alt, super]
```

### `desktop_type`

Type text into an element.

```
Parameters:
  text: str - Text to type
  ref: str - Element to focus first (optional)
  element: str - Human description
  clear_first: bool - Ctrl+A, Delete before typing
  submit: bool - Press Enter after
```

### `desktop_key`

Press keyboard keys/shortcuts.

```
Parameters:
  key: str - Key name (Return, Tab, Escape, a, etc.)
  modifiers: [ctrl, shift, alt, super]
```

### `desktop_capabilities`

Check available automation capabilities.

## Testing Workflow

To compile and test the application, follow these steps:

1. **Compile the application:**
   ```bash
   ninja -C build
   ```

2. **Run the application and inspect UI:**
   Run the application in the background and use `desktop_snapshot` to verify the UI tree:
   ```bash
   G_MESSAGES_DEBUG=all ./build/src/hdhomerun-config-gtk > app.log 2>&1 &
   # Then use: desktop_snapshot(app_name="hdhomerun-config-gtk")
   ```

3. **Verify Device Discovery:**
   The application automatically scans for HDHomeRun devices. Look for log messages like:
   - `DeviceList: Row selected for device [ID]`
   - `DeviceView: Refreshing [N] tuners`

