using System;
using Anvil.Net;

// Release probe for anvil-net (see scripts/release/check-grammar.sh).
Console.WriteLine($"version={AnvilDocument.GetVersion()}");
using (var inc = AnvilDocument.LoadBuffer("#!aml\ninclude \"missing.anvl\";\nx := 1;\n"))
    Console.WriteLine($"include={(inc is null ? "<null document>" : inc.HasErrors ? inc.Error?.Message : "<parsed without error>")}");
using (var doc = AnvilDocument.LoadBuffer("#!aml\nname := \"David\";\n"))
    Console.WriteLine($"name={doc?.FindValue("name")?.AsString() ?? "<none>"}");
