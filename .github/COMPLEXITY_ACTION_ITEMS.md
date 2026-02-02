# High Cyclomatic Complexity - Action Items

## Summary

Analysis identified **45 functions** with high cyclomatic complexity (CCN ≥ 15) in the Onion codebase. This document provides actionable recommendations for the development team.

## Critical Issues Requiring Immediate Attention

### 🔴 1. keymon/keymon.c - main() [CCN: 148]

**The Problem:**
- Extremely high complexity (148 CCN vs recommended 10)
- 439 lines in a single function
- 20+ state variables
- Massive switch statement
- Deep nesting (5 levels)

**Why It Matters:**
- Core system component (keyboard monitoring)
- High bug risk due to complexity
- Nearly impossible to test thoroughly
- Difficult to maintain and modify

**Immediate Actions:**

1. **Extract Button Handlers** (Priority: CRITICAL)
   ```c
   // Before: All in one switch
   switch (ev.code) {
       case HW_BTN_POWER: /* 50 lines */ break;
       case HW_BTN_MENU: /* 40 lines */ break;
       // ... 13 more cases
   }
   
   // After: Separate functions
   static void handle_power_button(uint32_t val, ButtonState *state);
   static void handle_menu_button(uint32_t val, ButtonState *state);
   static void handle_volume_buttons(uint32_t val, ButtonState *state);
   ```

2. **Create State Structure** (Priority: HIGH)
   ```c
   typedef struct {
       uint32_t button_flag;
       bool menu_pressed;
       bool power_pressed;
       bool comboKey_menu;
       // ... consolidate 20+ variables
   } KeymonState;
   ```

3. **Extract Event Loop** (Priority: HIGH)
   ```c
   int main(void) {
       keymon_init();
       keymon_event_loop();  // Extract 400+ lines
       keymon_cleanup();
       return 0;
   }
   ```

**Expected Impact:**
- Reduce CCN from 148 to ~30
- Improve testability significantly
- Make code maintainable

---

### 🔴 2. infoPanel/infoPanel.c - main() [CCN: 111]

**The Problem:**
- Very high complexity (111 CCN)
- 290 lines of mixed UI and logic
- Complex navigation state machine
- Device-specific conditionals

**Immediate Actions:**

1. **Extract Page Renderers**
   ```c
   static void render_main_page(void);
   static void render_system_info(void);
   static void render_storage_info(void);
   static void render_network_info(void);
   ```

2. **Create Navigation Module**
   ```c
   typedef enum {
       PAGE_MAIN,
       PAGE_SYSTEM,
       PAGE_STORAGE,
       PAGE_NETWORK
   } InfoPage;
   
   static void handle_navigation(InfoPage *current, int key);
   ```

**Expected Impact:**
- Reduce CCN from 111 to ~25
- Clearer code organization

---

### 🔴 3. prompt/prompt.c - main() [CCN: 67]

**The Problem:**
- High complexity in user prompt system
- Multiple prompt types handled in one function
- Mixed validation and UI logic

**Immediate Actions:**

1. **Create Prompt Type Handlers**
   ```c
   static int handle_yes_no_prompt(PromptConfig *config);
   static int handle_input_prompt(PromptConfig *config);
   static int handle_selection_prompt(PromptConfig *config);
   ```

2. **Extract Validation**
   ```c
   static bool validate_input(const char *input, PromptType type);
   ```

**Expected Impact:**
- Reduce CCN from 67 to ~20
- Reusable prompt components

---

### 🔴 4. tweaks/tweaks.c - main() [CCN: 64]

**The Problem:**
- High complexity in settings UI
- Large menu navigation switch
- Complex state management

**Immediate Actions:**

1. **Extract Menu Handlers**
   ```c
   typedef void (*MenuItemHandler)(void);
   
   static MenuItemHandler menu_handlers[] = {
       handle_display_settings,
       handle_audio_settings,
       handle_power_settings,
       // ...
   };
   ```

2. **Separate Menu from Actions**
   ```c
   static void render_menu(MenuItem *items, int count);
   static void execute_menu_action(int item_id);
   ```

**Expected Impact:**
- Reduce CCN from 64 to ~20
- Extensible menu system

---

## Quick Wins - Easy Refactorings

### 1. Replace Deep Nesting with Early Returns

**Before:**
```c
if (condition1) {
    if (condition2) {
        if (condition3) {
            // do something
        }
    }
}
```

**After:**
```c
if (!condition1) return;
if (!condition2) return;
if (!condition3) return;
// do something
```

**Files to Apply:** All high-CCN functions  
**CCN Reduction:** 2-5 points per function  
**Effort:** Low

---

### 2. Extract Helper Functions

**Pattern:** Any code block > 10 lines doing one thing

**Example from keymon.c:**
```c
// Before: Inline brightness adjustment (15 lines)
if (settings.brightness < MAX_BRIGHTNESS) {
    settings_setBrightness(settings.brightness + 1, true, false);
    settings_changed = true;
}
osd_showBrightnessBar(settings.brightness);
comboKey_select = true;

// After: Extract to function
adjust_brightness(+1, &settings_changed, &comboKey_select);
```

**Files to Apply:** Top 10 functions  
**CCN Reduction:** 3-10 points per function  
**Effort:** Low to Medium

---

### 3. Use Function Pointer Tables

**Pattern:** Large switch statements for similar actions

**Before:**
```c
switch (button) {
    case BTN_A: action_a(); break;
    case BTN_B: action_b(); break;
    case BTN_X: action_x(); break;
    case BTN_Y: action_y(); break;
    // ... 10+ more cases
}
```

**After:**
```c
typedef void (*ButtonHandler)(void);
static ButtonHandler handlers[] = {
    [BTN_A] = action_a,
    [BTN_B] = action_b,
    [BTN_X] = action_x,
    [BTN_Y] = action_y,
};

if (button < ARRAY_SIZE(handlers) && handlers[button]) {
    handlers[button]();
}
```

**Files to Apply:** keymon.c, keystateHandler.h  
**CCN Reduction:** 10-20 points  
**Effort:** Medium

---

## Refactoring Checklist

Use this checklist for each high-complexity function:

- [ ] **Document Current Behavior**
  - [ ] Write down what the function does
  - [ ] Create flowchart if complex
  - [ ] List all edge cases

- [ ] **Create Tests**
  - [ ] Write integration tests for current behavior
  - [ ] Create test harness
  - [ ] Establish baseline (all tests pass)

- [ ] **Identify Extraction Candidates**
  - [ ] Find cohesive code blocks
  - [ ] Look for repeated patterns
  - [ ] Identify single-responsibility sections

- [ ] **Refactor Incrementally**
  - [ ] Extract one function at a time
  - [ ] Run tests after each change
  - [ ] Commit working code frequently

- [ ] **Validate**
  - [ ] All tests still pass
  - [ ] Manual testing on device
  - [ ] Code review
  - [ ] Performance check

- [ ] **Document**
  - [ ] Add function comments
  - [ ] Update documentation
  - [ ] Note any behavior changes

---

## Testing Strategy

### Before Refactoring

1. **Characterization Tests** - Capture current behavior
   ```bash
   # Run on actual device and record:
   - Button press sequences
   - State transitions
   - UI responses
   - Edge cases
   ```

2. **Create Test Scripts**
   ```bash
   # Automated tests where possible
   ./test_keymon.sh
   ./test_navigation.sh
   ```

### During Refactoring

1. **Unit Tests** - For extracted functions
2. **Integration Tests** - For main flows
3. **Regression Tests** - Run after each change

### After Refactoring

1. **Full Test Suite** - All automated tests
2. **Manual Testing** - On actual hardware
3. **Performance Testing** - Ensure no degradation

---

## Resources

### Refactoring Techniques
- **Extract Function** - Most commonly needed
- **Replace Nested Conditionals** - Early returns, guard clauses
- **Replace Switch with Strategy** - Function pointers, dispatch tables
- **Extract State Machine** - For complex state management

### Tools
- **Lizard** - Complexity analysis (already installed)
- **cppcheck** - Static analysis
- **GDB** - Debugging during refactoring
- **Git** - Version control for safe refactoring

### Reference Materials
- Refactoring: Improving the Design of Existing Code (Martin Fowler)
- Clean Code (Robert C. Martin)
- Code Complete (Steve McConnell)

---

## Success Metrics

### Target Goals

| Metric | Current | Target | Status |
|--------|---------|--------|--------|
| Functions with CCN ≥ 50 | 4 | 0 | 🔴 |
| Functions with CCN ≥ 30 | 13 | < 5 | 🔴 |
| Functions with CCN ≥ 15 | 45 | < 20 | 🔴 |
| Average CCN | ~12 | < 10 | 🟡 |

### Progress Tracking

Create a tracking issue with checkboxes:

```markdown
## Refactoring Progress

### Critical (CCN ≥ 50)
- [ ] keymon/keymon.c: main() - CCN 148 → Target < 30
- [ ] infoPanel/infoPanel.c: main() - CCN 111 → Target < 30
- [ ] prompt/prompt.c: main() - CCN 67 → Target < 20
- [ ] tweaks/tweaks.c: main() - CCN 64 → Target < 20

### Very High (CCN 30-49)
- [ ] keystateHandler() - CCN 61 → Target < 20
- [ ] _loadImage() - CCN 54 → Target < 20
- [ ] isOutlinePixel() - CCN 49 → Target < 15
- [ ] theme_renderListCustom() - CCN 48 → Target < 20
- [ ] themeSwitcher main() - CCN 45 → Target < 20
```

---

## Next Steps

1. **Review This Report** - Team discussion, prioritize functions
2. **Create Task Breakdown** - Assign owners to top 4 functions
3. **Set Timeline** - Allocate time for refactoring
4. **Start with Tests** - Write tests before refactoring
5. **Refactor Incrementally** - One function at a time
6. **Track Progress** - Update metrics weekly
7. **Celebrate Wins** - When complexity reduced

---

## Questions or Concerns?

- **"Will refactoring break things?"** - No, if we test thoroughly first
- **"How long will this take?"** - 2-3 months for top priority items
- **"Is it worth the effort?"** - Yes, long-term maintenance will be much easier
- **"Can we do this incrementally?"** - Yes, that's the recommended approach

---

**Document Created:** 2026-02-02  
**Author:** Code Review Expert  
**Next Review:** After completing top 4 functions
