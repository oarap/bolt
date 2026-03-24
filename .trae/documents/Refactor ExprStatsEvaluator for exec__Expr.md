# Transition to exec::Expr-based Stats Evaluation

You are correct. We should align with the execution layer (`exec::Expr` and `ExprSet`) used in `HiveDataSource::next` rather than the intermediate representation (`core::ITypedExpr`). This ensures consistency in how expressions are interpreted and allows us to leverage the compiled expression tree.

## Plan

### Phase 1: Implement `ExprStatsEvaluator` for `exec::Expr`
1.  **Refactor `ExprStatsEvaluator.h`**:
    *   Change input type from `core::ITypedExpr` to `exec::Expr`.
    *   Remove `ITypedExpr` dependencies.
2.  **Refactor `ExprStatsEvaluator.cpp`**:
    *   Implement tree traversal using `exec::Expr::inputs()`.
    *   Handle `exec::Expr` types: `ConstantExpr`, `FieldReference`, `CallExpr`.
    *   Re-implement the Interval Arithmetic logic (Min/Max/Bloom checks) adapted for the `exec::Expr` structure.
3.  **Update Unit Tests**:
    *   Modify `ExprStatsEvaluatorTest.cpp` to construct `ExprSet` using `SimpleExpressionEvaluator`.
    *   Verify stats pruning logic works on the compiled expression tree.

### Phase 2: Integration
1.  **Modify `ScanSpec`**:
    *   Introduce `ExprSet` (or `std::vector<std::shared_ptr<exec::Expr>>`) to store pruning expressions.
    *   Use `ExprStatsEvaluator` to evaluate these expressions against row group statistics.
2.  **Parallel Execution**:
    *   Run `ExprStatsEvaluator` alongside the existing `Filter` logic to verify correctness.

### Phase 3: Migration
1.  **Switch Source**:
    *   Populate `ScanSpec` from the query plan's `ExprSet` instead of `Filter` objects.
    *   Remove legacy `Filter` code paths.

I will now proceed with **Phase 1**: Refactoring `ExprStatsEvaluator` to use `exec::Expr`.
