import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

/**
 * Emits a small, reproducible function inventory for the local reference
 * binaries. It records names and addresses only; it never copies code or
 * proprietary resources into the repository.
 */
public class DumpYwFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("program=" + currentProgram.getName());
        println("language=" + currentProgram.getLanguageID());
        println("compiler=" + currentProgram.getCompilerSpec().getCompilerSpecID());
        int total = 0;
        int selected = 0;
        for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
            total++;
            final String name = function.getName();
            final String lower = name.toLowerCase();
            if (lower.contains("yw") || lower.contains("lxp") || lower.contains("resource") ||
                lower.contains("sound") || lower.contains("map") || lower.contains("battle") ||
                lower.contains("chara") || lower.contains("event") || lower.contains("world") ||
                lower.contains("jni") || lower.contains("main")) {
                selected++;
                println("function=0x" + function.getEntryPoint().toString() + "\t" + name);
            }
        }
        println("function_count=" + total);
        println("selected_count=" + selected);
    }
}
