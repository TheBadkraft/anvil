Based on the sigcore String and StringBuilder APIs I found, here are several options that incorporate these libraries for pre-written error messages with positional formatting:

## Option 1: StringBuilder-Based Error Formatting

Leverage StringBuilder's `appendf` for printf-style formatting with automatic memory management:

```c
// In errors.c - add formatted error templates
typedef struct {
    anvl_error_code code;
    const char *template;  // printf-style format string
} error_template;

static const error_template error_templates[] = {
    {ANVL_ERR_PARSER_UNEXPECTED_TOKEN, "Unexpected token '%s' at line %d:%d, expected %s"},
    {ANVL_ERR_PARSER_INVALID_IDENTIFIER, "Invalid identifier '%s' at line %d:%d - %s"},
    {ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT, "Duplicate field '%s' in object at line %d:%d (first defined at line %d)"},
    // ...
};

// New function using StringBuilder
char* anvl_error_format_message(anvl_error_code code, usize line, usize col, ...) {
    string_builder sb = StringBuilder.new(256);  // Start with reasonable capacity
    
    // Find template
    const char *template = NULL;
    for (size_t i = 0; i < sizeof(error_templates)/sizeof(error_templates[0]); i++) {
        if (error_templates[i].code == code) {
            template = error_templates[i].template;
            break;
        }
    }
    
    if (template) {
        va_list args;
        va_start(args, col);
        StringBuilder.appendf(sb, template, args);  // Format with positional args
        va_end(args);
    } else {
        StringBuilder.append(sb, "Unknown error");
    }
    
    string result = StringBuilder.toString(sb);
    StringBuilder.dispose(sb);
    return result;  // Caller must dispose with String.dispose()
}
```

**Usage:**
```c
char *msg = anvl_error_format_message(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, 
                                     line, col, token_text, expected_desc);
anvl_error_set(code, msg, line, col, __FILE__);
String.dispose(msg);
```

## Option 2: String.format Integration

Use String.format for template-based error messages:

```c
// Enhanced error messages using String.format
static const char *error_templates[] = {
    [ANVL_ERR_PARSER_EXPECTED_IDENTIFIER] = "Expected identifier, found '%s' at line %d:%d",
    [ANVL_ERR_PARSER_INVALID_IDENTIFIER] = "Invalid identifier '%s' at line %d:%d: %s",
    [ANVL_ERR_PARSER_DUPLICATE_FIELD_IN_OBJECT] = "Duplicate field '%s' in object at line %d:%d",
    // ...
};

char* anvl_error_build_message(anvl_error_code code, usize line, usize col, ...) {
    const char *template = (code < sizeof(error_templates)/sizeof(error_templates[0])) 
                          ? error_templates[code] : "Unknown error";
    
    va_list args;
    va_start(args, col);
    string formatted = String.format(template, args);  // Uses String.format
    va_end(args);
    
    return formatted;  // String.format handles memory allocation
}
```

**Usage:**
```c
string msg = anvl_error_build_message(ANVL_ERR_PARSER_INVALID_IDENTIFIER, 
                                    line, col, identifier, reason);
anvl_error_set(code, msg, line, col, __FILE__);
// No need to dispose - String.format result is managed differently
```

## Option 3: Token-Aware Error Builder with StringBuilder

Create a comprehensive error builder that automatically includes token context:

```c
typedef struct {
    string_builder sb;
    bool has_position;
    usize line, col;
} error_builder;

static error_builder* error_builder_new(void) {
    error_builder *eb = Memory.alloc(sizeof(error_builder), false);
    eb->sb = StringBuilder.new(128);
    eb->has_position = false;
    return eb;
}

static void error_builder_set_position(error_builder *eb, usize line, usize col) {
    eb->line = line;
    eb->col = col;
    eb->has_position = true;
}

static void error_builder_add_token(error_builder *eb, token *tok) {
    if (tok && tok->text) {
        StringBuilder.appendf(eb->sb, "'%s'", tok->text);
    }
}

static void error_builder_add_expected(error_builder *eb, const char *expected) {
    StringBuilder.appendf(eb->sb, ", expected %s", expected);
}

static char* error_builder_build(error_builder *eb, anvl_error_code code) {
    // Could prepend error code message or additional context
    string result = StringBuilder.toString(eb->sb);
    StringBuilder.dispose(eb->sb);
    Memory.dispose(eb);
    return result;
}
```

**Usage:**
```c
error_builder *eb = error_builder_new();
error_builder_set_position(eb, line, col);
StringBuilder.append(eb->sb, "Unexpected token ");
error_builder_add_token(eb, current_token);
error_builder_add_expected(eb, "expression");

char *msg = error_builder_build(eb, ANVL_ERR_PARSER_UNEXPECTED_TOKEN);
anvl_error_set(code, msg, line, col, __FILE__);
String.dispose(msg);
```

## Option 4: Structured Error Context with StringBuilder

Build error messages incrementally with context accumulation:

```c
typedef struct {
    anvl_error_code code;
    string_builder sb;
    usize line, col;
    bool has_token;
    string token_text;
    string expected;
} error_context;

static error_context* error_context_new(anvl_error_code code, usize line, usize col) {
    error_context *ctx = Memory.alloc(sizeof(error_context), false);
    ctx->code = code;
    ctx->sb = StringBuilder.new(256);
    ctx->line = line;
    ctx->col = col;
    ctx->has_token = false;
    ctx->token_text = NULL;
    ctx->expected = NULL;
    return ctx;
}

static void error_context_set_token(error_context *ctx, string token) {
    ctx->has_token = true;
    ctx->token_text = String.copy(token);
}

static void error_context_set_expected(error_context *ctx, string expected) {
    ctx->expected = String.copy(expected);
}

static char* error_context_format(error_context *ctx) {
    // Base message from error code
    const char *base_msg = anvl_error_code_message(ctx->code);
    StringBuilder.append(ctx->sb, base_msg);
    
    if (ctx->has_token) {
        StringBuilder.appendf(ctx->sb, ": '%s'", ctx->token_text);
        String.dispose(ctx->token_text);
    }
    
    if (ctx->expected) {
        StringBuilder.appendf(ctx->sb, " (expected %s)", ctx->expected);
        String.dispose(ctx->expected);
    }
    
    string result = StringBuilder.toString(ctx->sb);
    StringBuilder.dispose(ctx->sb);
    Memory.dispose(ctx);
    return result;
}
```

**Usage:**
```c
error_context *ctx = error_context_new(ANVL_ERR_PARSER_UNEXPECTED_TOKEN, line, col);
error_context_set_token(ctx, token->text);
error_context_set_expected(ctx, "identifier");

string msg = error_context_format(ctx);
anvl_error_set(code, msg, line, col, __FILE__);
// msg disposal depends on StringBuilder.toString implementation
```

## Option 5: Hybrid Approach with Template Expansion

Combine pre-written templates with StringBuilder for complex formatting:

```c
typedef struct {
    const char *template;
    bool needs_token;
    bool needs_expected;
    bool needs_position;
} error_spec;

static const error_spec error_specs[] = {
    [ANVL_ERR_PARSER_UNEXPECTED_TOKEN] = {
        .template = "Unexpected token %s%s at %s",
        .needs_token = true,
        .needs_expected = true,
        .needs_position = true
    },
    [ANVL_ERR_PARSER_INVALID_IDENTIFIER] = {
        .template = "Invalid identifier %s at %s: %s",
        .needs_token = true,
        .needs_expected = false,
        .needs_position = true
    }
};

string anvl_error_expand_template(anvl_error_code code, usize line, usize col, 
                                 string token_text, string expected, string reason) {
    const error_spec *spec = &error_specs[code];
    string_builder sb = StringBuilder.new(128);
    
    // Build position string if needed
    string pos_str = NULL;
    if (spec->needs_position) {
        pos_str = String.format("line %d:%d", line, col);
    }
    
    // Expand template with available components
    if (spec->needs_token && spec->needs_expected && spec->needs_position) {
        StringBuilder.appendf(sb, spec->template, token_text, expected, pos_str);
    } else if (spec->needs_token && spec->needs_position) {
        StringBuilder.appendf(sb, spec->template, token_text, pos_str, reason);
    }
    // ... other combinations
    
    if (pos_str) String.dispose(pos_str);
    
    string result = StringBuilder.toString(sb);
    StringBuilder.dispose(sb);
    return result;
}
```

Each option leverages sigcore's String and StringBuilder APIs differently - Option 1 uses StringBuilder for printf-style formatting, Option 2 uses String.format for template expansion, while Options 3-5 create structured builders that accumulate context before final formatting. The StringBuilder approach (Option 1) would be most flexible for complex error messages with multiple positional parameters.

---

Given that we have the beginnings of error reporting already established, here are options that provide implementations for automatically formatting appropriate error messages with positional information ...

Read [](file:///home/badkraft/repos/anvil/src/core/parser.c#60-60), lines 60 to 70

Searched for text `parser_error\(` (`**/src/core/parser.c`), 20 results

Based on the current error handling, here are several approaches for automatically determining when to format positional information into error messages:

## Option 1: Error Code Categories with Automatic Formatting

Add metadata to error codes that determines formatting behavior:

```c
typedef enum {
    ERR_FORMAT_PLAIN,        // Just the message
    ERR_FORMAT_POSITIONAL,   // Message + "at line X:Y"
    ERR_FORMAT_TOKEN,        // Message + token info + position
    ERR_FORMAT_CUSTOM        // Custom formatting needed
} error_format_type;

typedef struct {
    anvl_error_code code;
    error_format_type format_type;
} error_metadata;

static const error_metadata error_metadata[] = {
    {ANVL_ERR_PARSER_EXPECTED_IDENTIFIER, ERR_FORMAT_POSITIONAL},
    {ANVL_ERR_PARSER_UNEXPECTED_TOKEN, ERR_FORMAT_TOKEN},
    {ANVL_ERR_MEMORY_ALLOCATION_FAILED, ERR_FORMAT_PLAIN},
    {ANVL_ERR_INVALID_ARGUMENT, ERR_FORMAT_PLAIN},
    // ...
};

// Enhanced error_set that formats automatically
void anvl_error_set(anvl_error_code code, const char *message, usize line, usize column, const char *file) {
    const char *final_message = message;
    
    // Find metadata for this error code
    error_format_type format = ERR_FORMAT_PLAIN;
    for (size_t i = 0; i < sizeof(error_metadata)/sizeof(error_metadata[0]); i++) {
        if (error_metadata[i].code == code) {
            format = error_metadata[i].format_type;
            break;
        }
    }
    
    // Auto-format based on type
    if (format == ERR_FORMAT_POSITIONAL && (line > 0 || column > 0)) {
        string formatted = String.format("%s at line %d:%d", message, line, column);
        final_message = formatted;
        // Store formatted message (would need disposal logic)
    }
    
    global_error_state.code = code;
    global_error_state.message = final_message;
    global_error_state.line = line;
    global_error_state.column = column;
    global_error_state.file = file;
}
```

## Option 2: Smart Parser Error Function

Modify the parser error function to automatically include positional info for parser errors:

```c
// Enhanced parser_error that automatically formats positional info
static void parser_error(anvl_error_code code, usize line, usize col) {
    const char *base_message = anvl_error_code_message(code);
    
    // All parser errors get positional formatting
    if (line > 0 || col > 0) {
        string formatted = String.format("%s at line %d:%d", base_message, line, col);
        anvl_error_set(code, formatted, line, col, __FILE__);
        String.dispose(formatted);
    } else {
        anvl_error_set(code, base_message, line, col, __FILE__);
    }
}

// For non-parser errors, use plain messages
void anvl_error_set_plain(anvl_error_code code, const char *message, usize line, usize column, const char *file) {
    global_error_state.code = code;
    global_error_state.message = message;
    global_error_state.line = line;
    global_error_state.column = column;
    global_error_state.file = file;
}
```

## Option 3: Error Code Range Detection

Automatically format based on error code ranges:

```c
static bool error_code_needs_position(anvl_error_code code) {
    // Parser errors (2000-4999) always include position
    if (code >= 2000 && code < 5000) return true;
    
    // Specific non-parser errors that benefit from position
    switch (code) {
        case ANVL_ERR_INVALID_ARGUMENT:
        case ANVL_ERR_INVALID_OPERATION:
            return false; // These are typically programmatic errors
        default:
            return false;
    }
}

void anvl_error_set(anvl_error_code code, const char *message, usize line, usize column, const char *file) {
    const char *final_message = message;
    
    if (error_code_needs_position(code) && (line > 0 || column > 0)) {
        string_builder sb = StringBuilder.new(strlen(message) + 32);
        StringBuilder.append(sb, message);
        StringBuilder.appendf(sb, " at line %d:%d", line, column);
        final_message = StringBuilder.toString(sb);
        StringBuilder.dispose(sb);
        // Need disposal tracking for final_message
    }
    
    global_error_state.code = code;
    global_error_state.message = final_message;
    global_error_state.line = line;
    global_error_state.column = column;
    global_error_state.file = file;
}
```

## Option 4: Template-Based System with Auto-Expansion

Use templates that automatically expand positional information:

```c
// Templates with placeholders for automatic expansion
static const char *error_templates[] = {
    [ANVL_ERR_PARSER_EXPECTED_IDENTIFIER] = "Expected identifier{pos}",
    [ANVL_ERR_PARSER_UNEXPECTED_TOKEN] = "Unexpected token{pos}",
    [ANVL_ERR_MEMORY_ALLOCATION_FAILED] = "Memory allocation failed", // No {pos}
    [ANVL_ERR_INVALID_ARGUMENT] = "Invalid argument", // No {pos}
};

const char *anvl_error_code_message(anvl_error_code code) {
    if (code >= sizeof(error_templates) / sizeof(error_templates[0])) {
        return "Unknown error";
    }
    return error_templates[code] ? error_templates[code] : "Unknown error";
}

// Enhanced error_set that expands templates
void anvl_error_set(anvl_error_code code, const char *message, usize line, usize column, const char *file) {
    const char *final_message = message;
    
    // Check if message contains {pos} placeholder
    if (strstr(message, "{pos}")) {
        char pos_str[32];
        if (line > 0 || column > 0) {
            snprintf(pos_str, sizeof(pos_str), " at line %d:%d", line, column);
        } else {
            pos_str[0] = '\0';
        }
        
        // Replace {pos} with position string
        string expanded = String.dupe(message);
        // String replacement logic here...
        final_message = expanded;
    }
    
    global_error_state.code = code;
    global_error_state.message = final_message;
    global_error_state.line = line;
    global_error_state.column = column;
    global_error_state.file = file;
}
```

## Option 5: Dual Interface Approach

Provide both formatted and unformatted error setting:

```c
// Internal function that handles formatting logic
static const char* format_error_message(anvl_error_code code, const char *base_message, 
                                       usize line, usize col) {
    // Parser errors automatically get positional info
    if (code >= ANVL_ERR_PARSER_EXPECTED_IDENTIFIER && 
        code <= ANVL_ERR_PARSER_INVALID_NUMBER && 
        (line > 0 || col > 0)) {
        
        return String.format("%s at line %d:%d", base_message, line, col);
    }
    
    return String.dupe(base_message); // Ensure owned copy
}

// Public interface - automatically formats based on error type
void anvl_error_set(anvl_error_code code, const char *message, usize line, usize column, const char *file) {
    const char *formatted_message = format_error_message(code, message, line, column);
    
    global_error_state.code = code;
    global_error_state.message = formatted_message;
    global_error_state.line = line;
    global_error_state.column = column;
    global_error_state.file = file;
}

// Alternative interface for explicit control
void anvl_error_set_exact(anvl_error_code code, const char *exact_message, usize line, usize column, const char *file) {
    global_error_state.code = code;
    global_error_state.message = exact_message;
    global_error_state.line = line;
    global_error_state.column = column;
    global_error_state.file = file;
}
```

**Option 1** (Error Code Categories) gives you the most control but requires maintaining metadata. **Option 3** (Error Code Range Detection) is the simplest - parser errors automatically get positional formatting, while other errors remain plain. **Option 5** (Dual Interface) maintains backward compatibility while adding automatic formatting.

Which approach aligns best with how you want the error system to behave?
