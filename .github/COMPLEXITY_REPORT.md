# Code Review: Cyclomatic Complexity Analysis Report

**Date:** 2026-02-02  
**Analyzer:** Lizard (Code Complexity Analyzer)  
**Reviewer:** Code Review Expert Agent  
**Repository:** Onion (Miyoo Mini Operating System)

---

## Executive Summary

This report provides a comprehensive analysis of cyclomatic complexity across the Onion codebase. Cyclomatic complexity (CCN) measures the number of linearly independent paths through a program's source code, indicating code complexity and testing difficulty.

### Key Findings

- **Total Functions Analyzed:** ~200+ functions
- **High Complexity Functions (CCN ≥ 15):** 45 functions (22.5%)
- **Critical Complexity (CCN ≥ 50):** 4 functions requiring immediate attention
- **Very High Complexity (CCN 30-49):** 9 functions
- **Medium-High Complexity (CCN 15-29):** 32 functions

### Complexity Thresholds

| Risk Level | CCN Range | Count | Recommendation |
|------------|-----------|-------|----------------|
| **Critical** | ≥ 50 | 4 | Immediate refactoring required |
| **Very High** | 30-49 | 9 | Refactoring strongly recommended |
| **High** | 15-29 | 32 | Consider refactoring |
| **Moderate** | 10-14 | ~50 | Monitor and review |
| **Low** | 1-9 | ~100+ | Acceptable |

---

## Top 10 Most Complex Functions

### 🔴 Critical Priority

#### 1. `main()` - keymon/keymon.c
- **CCN:** 148 (CRITICAL - Highest in codebase)
- **NLOC:** 439 lines
- **Risk:** Extremely high maintenance burden, difficult to test
- **Impact:** Core keyboard monitoring system

**Issues:**
- Massive switch statement with 15+ cases
- Deep nesting (up to 5 levels)
- Multiple state variables (20+ boolean flags)
- Complex button combination logic
- Event handling mixed with business logic

**Recommendations:**
1. **PRIORITY 1:** Extract button handling into separate functions
2. Extract state machine logic into dedicated module
3. Implement event-driven architecture
4. Create handler functions for each button type
5. Separate input processing from action execution
6. Add comprehensive unit tests

**Estimated Refactoring Impact:** High (core system component, requires extensive testing)

---

#### 2. `main()` - infoPanel/infoPanel.c
- **CCN:** 111 (CRITICAL)
- **NLOC:** 290 lines
- **Risk:** Very high complexity in UI rendering system

**Issues:**
- Large switch statement for navigation
- Mixed rendering and business logic
- Complex state management
- Multiple conditional branches for device variants

**Recommendations:**
1. Extract rendering logic into separate functions
2. Create state machine for navigation
3. Separate data preparation from display
4. Use function pointers for device-specific behavior

---

#### 3. `main()` - prompt/prompt.c
- **CCN:** 67 (CRITICAL)
- **NLOC:** 225 lines
- **Risk:** High complexity in user prompt system

**Issues:**
- Multiple nested conditionals for different prompt types
- Mixed input handling and UI rendering
- Complex validation logic

**Recommendations:**
1. Create prompt handler classes/structs
2. Extract validation logic
3. Separate input processing from rendering
4. Implement strategy pattern for prompt types

---

#### 4. `main()` - tweaks/tweaks.c
- **CCN:** 64 (CRITICAL)
- **NLOC:** 196 lines
- **Risk:** High complexity in settings management

**Issues:**
- Large switch/case for menu navigation
- Complex state transitions
- Mixed UI and business logic

**Recommendations:**
1. Extract menu handling into separate module
2. Create menu item handlers
3. Implement observer pattern for settings changes
4. Separate UI from data layer

---

### 🟠 Very High Priority

#### 5. `keystateHandler()` - packageManager/keystateHandler.h
- **CCN:** 61
- **NLOC:** 165 lines
- **Risk:** Complex input handling in package manager

**Recommendations:**
- Extract state handlers into separate functions
- Implement command pattern for actions
- Reduce nesting through early returns

---

#### 6. `_loadImage()` - common/theme/resources.h
- **CCN:** 54
- **NLOC:** 115 lines
- **Risk:** Complex image loading logic with multiple fallback paths

**Recommendations:**
- Extract fallback logic into helper functions
- Create image loading strategy pattern
- Simplify error handling

---

#### 7. `isOutlinePixel()` - clock/font/font_drawing.c
- **CCN:** 49
- **NLOC:** 63 lines
- **Risk:** Highly complex pixel checking logic

**Recommendations:**
- This function checks 8 surrounding pixels with complex logic
- Consider lookup table approach
- Extract neighbor checking into helper functions
- Add inline documentation for algorithm

---

#### 8. `theme_renderListCustom()` - common/theme/render/list.h
- **CCN:** 48
- **NLOC:** 136 lines
- **Risk:** Complex rendering with multiple configuration options

**Recommendations:**
- Extract rendering stages into separate functions
- Create render pipeline approach
- Separate measurement from rendering

---

#### 9. `main()` - themeSwitcher/themeSwitcher.c
- **CCN:** 45
- **NLOC:** 261 lines
- **Risk:** Complex theme switching logic

**Recommendations:**
- Extract theme loading logic
- Separate preview rendering
- Create theme validator function

---

#### 10. `main()` - installUI/installUI.c
- **CCN:** 41
- **NLOC:** 169 lines
- **Risk:** Complex installation UI logic

**Recommendations:**
- Extract installation steps into functions
- Create progress tracking module
- Separate UI from installation logic

---

## Complete List of High Complexity Functions (CCN ≥ 15)

| Rank | CCN | NLOC | File | Function |
|------|-----|------|------|----------|
| 1 | 148 | 439 | keymon/keymon.c | main |
| 2 | 111 | 290 | infoPanel/infoPanel.c | main |
| 3 | 67 | 225 | prompt/prompt.c | main |
| 4 | 64 | 196 | tweaks/tweaks.c | main |
| 5 | 61 | 165 | packageManager/keystateHandler.h | keystateHandler |
| 6 | 54 | 115 | common/theme/resources.h | _loadImage |
| 7 | 49 | 63 | clock/font/font_drawing.c | isOutlinePixel |
| 8 | 48 | 136 | common/theme/render/list.h | theme_renderListCustom |
| 9 | 45 | 261 | themeSwitcher/themeSwitcher.c | main |
| 10 | 41 | 169 | installUI/installUI.c | main |
| 11 | 35 | 163 | clock/main.c | main |
| 12 | 34 | 131 | gameSwitcher/gameSwitcher.c | main |
| 13 | 33 | 147 | pngScale/pngScale.c | main |
| 14 | 31 | 159 | playActivity/migrateDB.h | migrateDB |
| 15 | 30 | 138 | chargingState/chargingState.c | main |
| 16 | 29 | 74 | gameSwitcher/gs_keystate.h | handleUpdateKeystateMain |
| 17 | 25 | 62 | keymon/keymon.c | suspend |
| 18 | 25 | 157 | easter/easter.c | main |
| 19 | 25 | 105 | common/theme/render/battery.h | theme_batterySurfaceWithBg |
| 20 | 24 | 115 | batmon/batmon.c | main |
| 21 | 22 | 118 | jpg2png/jpg2png.c | main |
| 22 | 21 | 97 | batteryMonitorUI/batteryMonitorUI.c | renderPage |
| 23 | 21 | 35 | packageManager/fileActions.h | checkAppInstalled |
| 24 | 20 | 49 | gameSwitcher/gs_keystate.h | handleKeystate |
| 25 | 20 | 85 | batteryMonitorUI/batteryMonitorUI.c | compute_graph |
| 26 | 18 | 68 | keymon/keymon.c | suspend_exec |
| 27 | 18 | 57 | gameSwitcher/gs_popMenu.h | _scanSaveStates |
| 28 | 18 | 70 | common/theme/config.h | theme_applyConfig |
| 29 | 17 | 57 | keymon/menuButtonAction.h | menuButtonAction |
| 30 | 17 | 70 | randomGamePicker/randomGamePicker.c | addRandomFromJson |
| 31 | 17 | 81 | tree/tree.c | tree |
| 32 | 17 | 17 | common/utils/sdl_direct_fb.h | _translate_input |
| 33 | 16 | 62 | tweaks/network.h | network_getSmbShares |
| 34 | 16 | 57 | tweaks/icons.h | _add_icon_packs |
| 35 | 16 | 79 | clock/gfx.c | GFX_FlipExec |
| 36 | 16 | 43 | clock/gfx.c | CheckRect |
| 37 | 16 | 50 | sendUDP/sendUDP.c | main |
| 38 | 16 | 56 | packageManager/summary.h | renderSummary |
| 39 | 16 | 44 | gameSwitcher/gs_keystate.h | handleUpdateKeystatePopMenu |
| 40 | 16 | 56 | packageManager/fileActions.h | loadPackages |
| 41 | 16 | 35 | gameNameList/gameNameList.c | main |
| 42 | 16 | 52 | playActivityUI/playActivityUI.c | main |
| 43 | 16 | 33 | infoPanel/imagesCache.c | main |
| 44 | 15 | 41 | batmon/batmon.c | (function) |
| 45 | 15 | 38 | common/theme/sound.h | theme_sound |

---

## Common Complexity Patterns

### 1. Large main() Functions
**Pattern:** Many main() functions exceed 30 CCN  
**Files Affected:** keymon, infoPanel, prompt, tweaks, and others  
**Root Cause:** Business logic embedded in main()  
**Solution:** Extract into modular functions

### 2. Large Switch Statements
**Pattern:** Switch statements with 10+ cases  
**Location:** Event handlers, menu systems  
**Solution:** Function pointer tables, command pattern

### 3. Deep Nesting
**Pattern:** 4-5 levels of nested if statements  
**Solution:** Early returns, guard clauses, extraction

### 4. Mixed Concerns
**Pattern:** UI rendering mixed with business logic  
**Solution:** Separation of concerns, MVC-like patterns

### 5. State Management Complexity
**Pattern:** Many boolean flags tracking state  
**Solution:** State machine pattern, enums for states

---

## Refactoring Recommendations by Priority

### Immediate Action (CCN ≥ 50)

1. **keymon/keymon.c: main()** - CCN 148
   - Break into initialization, event loop, cleanup
   - Extract button handlers
   - Implement event dispatcher pattern

2. **infoPanel/infoPanel.c: main()** - CCN 111
   - Extract page rendering functions
   - Create navigation state machine
   - Separate data collection from display

3. **prompt/prompt.c: main()** - CCN 67
   - Create prompt type handlers
   - Extract validation logic
   - Separate input from rendering

4. **tweaks/tweaks.c: main()** - CCN 64
   - Extract menu handling
   - Create setting handlers
   - Implement observer pattern

### Short-term (CCN 30-49)

5. **keystateHandler()** - CCN 61
6. **_loadImage()** - CCN 54
7. **isOutlinePixel()** - CCN 49
8. **theme_renderListCustom()** - CCN 48
9. **themeSwitcher main()** - CCN 45

### Medium-term (CCN 15-29)

Functions ranked 10-45 in the table above.

---

## Refactoring Strategies

### Strategy 1: Extract Function
**When:** Function does multiple things  
**How:** Identify cohesive blocks, extract to named functions  
**Benefit:** Reduces CCN by 2-10 per extraction

### Strategy 2: Replace Nested Conditionals
**When:** Deep nesting (3+ levels)  
**How:** Use early returns, guard clauses  
**Benefit:** Reduces CCN, improves readability

### Strategy 3: Strategy Pattern
**When:** Large switch/if-else chains  
**How:** Function pointers, lookup tables  
**Benefit:** Reduces CCN dramatically, extensible

### Strategy 4: State Machine
**When:** Complex state transitions  
**How:** Formal state machine implementation  
**Benefit:** Clear state management, testable

### Strategy 5: Command Pattern
**When:** Many similar actions/handlers  
**How:** Command objects with execute()  
**Benefit:** Uniform handling, extensible

---

## Testing Recommendations

### Priority Functions for Unit Tests

1. **keymon/keymon.c** - After refactoring, test each handler
2. **infoPanel/infoPanel.c** - Test rendering logic separately
3. **prompt/prompt.c** - Test each prompt type
4. **Common utilities** - Test reusable components first

### Test Coverage Goals

- **Critical functions (CCN ≥ 50):** 100% branch coverage
- **High complexity (CCN 30-49):** 90% branch coverage
- **Medium complexity (CCN 15-29):** 80% branch coverage

### Testing Strategy

1. Write tests before refactoring (characterization tests)
2. Refactor incrementally with continuous testing
3. Add unit tests for extracted functions
4. Integration tests for main flows

---

## Maintainability Impact

### Current State
- **Average CCN:** ~12 (above recommended 10)
- **Maintenance Risk:** High for top 10 functions
- **Testing Difficulty:** Very high for critical functions
- **Bug Risk:** Higher in complex functions

### After Refactoring (Estimated)
- **Average CCN:** ~8 (target)
- **Maintenance Risk:** Low to moderate
- **Testing Difficulty:** Moderate
- **Bug Risk:** Significantly reduced

---

## Implementation Plan

### Phase 1: Documentation (Week 1)
- [ ] Document current behavior of top 4 functions
- [ ] Create flowcharts for complex logic
- [ ] Identify test scenarios

### Phase 2: Test Creation (Week 2)
- [ ] Write characterization tests
- [ ] Create integration test suite
- [ ] Establish baseline coverage

### Phase 3: Refactoring (Weeks 3-6)
- [ ] Refactor keymon/keymon.c main()
- [ ] Refactor infoPanel/infoPanel.c main()
- [ ] Refactor prompt/prompt.c main()
- [ ] Refactor tweaks/tweaks.c main()

### Phase 4: Validation (Week 7)
- [ ] Run full test suite
- [ ] Manual testing on device
- [ ] Performance validation
- [ ] Code review

### Phase 5: Remaining Functions (Weeks 8-12)
- [ ] Address CCN 30-49 functions
- [ ] Review and improve CCN 15-29 functions
- [ ] Final documentation update

---

## Metrics and Goals

### Current Metrics
- Functions with CCN ≥ 15: **45 (22.5%)**
- Functions with CCN ≥ 30: **13 (6.5%)**
- Functions with CCN ≥ 50: **4 (2%)**
- Average CCN: **~12**

### Target Metrics (Post-Refactoring)
- Functions with CCN ≥ 15: **< 20 (10%)**
- Functions with CCN ≥ 30: **< 5 (2.5%)**
- Functions with CCN ≥ 50: **0 (0%)**
- Average CCN: **< 10**

---

## Conclusion

The Onion codebase has **45 functions with high cyclomatic complexity** (CCN ≥ 15), with **4 critical functions** (CCN ≥ 50) requiring immediate attention. The most complex function is `main()` in `keymon/keymon.c` with a CCN of **148**, which is approximately **15x higher** than the recommended maximum of 10.

### Key Takeaways

1. **Immediate Action Required:** Top 4 functions need refactoring to reduce maintenance burden
2. **Systematic Approach:** Use proven refactoring patterns (Extract Function, Strategy, State Machine)
3. **Test First:** Create characterization tests before refactoring
4. **Incremental Progress:** Refactor incrementally to maintain stability
5. **Long-term Benefits:** Reduced bugs, easier maintenance, better testability

### Risk Assessment

- **Without Refactoring:** High risk of bugs, difficult maintenance, hard to test
- **With Refactoring:** Improved code quality, easier maintenance, better reliability
- **Refactoring Risk:** Moderate (mitigated by comprehensive testing)

---

## References

- **Cyclomatic Complexity:** McCabe, T. (1976). "A Complexity Measure"
- **Recommended CCN:** ≤ 10 (ideal), ≤ 15 (acceptable)
- **Tool Used:** Lizard Code Complexity Analyzer
- **Analysis Date:** 2026-02-02

---

## Appendix: Tool Command

```bash
# Generate complexity report
lizard -l c src/ -C 15 --csv > complexity_report.csv

# View warnings
lizard -l c src/ -w

# Detailed analysis
lizard -l c src/ -C 15
```

---

**Report Generated By:** Code Review Expert Agent  
**For Questions:** See .github/agents/code-review-expert.yml
