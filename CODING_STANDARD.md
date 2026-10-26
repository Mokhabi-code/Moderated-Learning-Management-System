# MLMS C++ Coding Standard

- Target C++17; compile with extensions disabled.
- Use `PascalCase` for types and `camelCase` for functions and variables.
- Use `UPPER_SNAKE_CASE` for compile-time constants.
- Put declarations in `include/mlms/` and implementations in `src/`.
- Prefer small functions, explicit types, `const` correctness, and RAII.
- Represent expected domain failures with typed exceptions or explicit result types.
- Never concatenate user input into SQL. Bind all values through prepared statements.
- Services enforce business rules and authorization; menus are not security boundaries.
- Keep database transactions short and atomic. Roll back automatically on scope exit.
- Do not store plaintext passwords. Use a vetted Argon2id implementation.
- Test normal, boundary, invalid-input, authorization, and failure/rollback paths.
- Every mark change must produce an audit entry with actor, timestamp, old value, new value, and reason.
