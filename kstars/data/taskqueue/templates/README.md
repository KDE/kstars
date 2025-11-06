# Ekos Task Queue Templates

This directory contains system templates for the Ekos Task Queue system. Templates define reusable sequences of INDI device operations that can be customized by users.

## Template Files

### camera.json
Camera operations including:
- **Cool Camera**: Cools camera to target temperature with controlled ramping using CCD_TEMPERATURE and CCD_TEMP_RAMP
- **Warm Camera**: Warms camera to ambient temperature with controlled ramping

### mount.json
Mount operations including:
- **Park Mount**: Parks telescope mount to home position using TELESCOPE_PARK
- **Unpark Mount**: Unparks mount for observations

### dome.json
Dome operations including:
- **Park Dome**: Parks observatory dome using DOME_PARK
- **Unpark Dome**: Unparks dome for observations

### dustcap.json
Dust cap operations including:
- **Park Dust Cap**: Closes dust cap to protect optics using CAP_PARK
- **Unpark Dust Cap**: Opens dust cap for observations

## Template Structure

Each template file contains:

```json
{
    "category": "Device Category",
    "device_type": "Device Type",
    "version": "1.0",
    "templates": [
        {
            "id": "unique_template_id",
            "name": "Display Name",
            "description": "Description of what the template does",
            "version": "1.0",
            "category": "Device Category",
            "supported_interfaces": [4],
            "parameters": [
                {
                    "name": "parameter_name",
                    "type": "number|text|boolean",
                    "default": "default_value",
                    "min": 0,
                    "max": 100,
                    "step": 1,
                    "unit": "unit_string",
                    "description": "Parameter description"
                }
            ],
            "actions": [
                {
                    "type": "SET|EVALUATE|DELAY|START",
                    "property": "INDI_PROPERTY_NAME",
                    "element": "ELEMENT_NAME",
                    "value": "${parameter_name}",
                    "timeout": 30,
                    "retries": 2,
                    "failure_action": 0
                }
            ]
        }
    ]
}
```

## Action Types

### SET
Sets an INDI property element to a specific value.

### EVALUATE
Checks if an INDI property element or state meets a condition with polling.

**Property Types:**
- 0: number
- 1: text
- 2: switch
- 3: light
- 4: state

**Condition Types:**
- 0: equals
- 1: not_equals
- 2: greater_than
- 3: less_than
- 4: greater_equal
- 5: less_equal
- 6: within_range
- 7: contains
- 8: starts_with

### DELAY
Pauses execution for a fixed time period.

### START
Schedules when the task should begin execution.

## Failure Actions

- 0: ABORT_QUEUE - Stop entire queue execution
- 1: CONTINUE - Log error but continue to next action
- 2: SKIP_TO_NEXT_TASK - Skip remaining actions in current task

## INDI Device Interfaces

Common interface values:
- 1: TELESCOPE_INTERFACE (Mount)
- 4: CCD_INTERFACE (Camera)
- 8: DOME_INTERFACE (Dome)
- 32: DUSTCAP_INTERFACE (Dust Cap)

## Parameter Substitution

Templates support parameter substitution using `${parameter_name}` syntax. When a task is created from a template, these placeholders are replaced with actual parameter values.

Example:
```json
{
    "value": "${target_temperature}"
}
```

## Creating Custom Templates

### Using the Property Template Builder (GUI)

KStars provides a built-in Property Template Builder for creating custom templates without writing JSON manually:

1. Open the Template Library from the Task Queue
2. Click **"Create Custom Template"** button
3. Follow the 5-step wizard:
   - **Step 1: Device Selection** - Choose from connected INDI devices or cached profile devices
   - **Step 2: Property Selection** - Select any compatible property (NUMBER, SWITCH, TEXT, or LIGHT)
   - **Step 3: Action Configuration** - Configure SET and/or EVALUATE actions with property-specific controls
   - **Step 4: Template Metadata** - Provide name, description, and category
   - **Step 5: Preview and Save** - Review the generated JSON and save

The builder automatically:
- Filters incompatible properties (excludes BLOB types)
- Handles read-only properties (LIGHT types can only be evaluated)
- Provides property-specific UI controls:
  - **NUMBER**: Value input, condition selection (equals, less than, greater than, within range, etc.)
  - **SWITCH**: ON/OFF state selection
  - **TEXT**: Text input, condition selection (equals, contains)
  - **LIGHT**: State evaluation (Idle, Ok, Busy, Alert)
- Generates valid JSON with proper property type encoding
- Saves to user templates directory

User templates created with the builder are stored in: `~/.local/share/kstars/taskqueue/templates/user/`

### Manual Template Creation

Advanced users can also create custom templates manually by:
1. Duplicating an existing system template
2. Modifying parameter default values
3. Saving as a user template

## Loading Templates

Templates are automatically loaded by the TemplateManager singleton:
```cpp
TemplateManager::Instance()->initialize();
```

System templates are loaded from the KStars data directory, and user templates from the local config directory.
