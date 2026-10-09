# Getting Started

## Usage Instructions

1. **Copy the files** to your local machine.
2. **Modify input equation:** Update the `front.in` file with your desired expression.
   
   *Example `front.in`:*
   ```text
   x = 20 + 3
   ```

3. **Compile the program:**
   ```bash
   gcc front.c -o front.exe
   ```

4. **Run the executable:**
   ```bash
   ./front.exe
   ```

### Expected Output

```text
Next token is: 11, Next lexeme is x
Next token is: 20, Next lexeme is =
Next token is: 10, Next lexeme is 20
Next token is: 21, Next lexeme is +
Next token is: 10, Next lexeme is 3
Next token is: -1, Next lexeme is EOF
```
