#include "llvm/Support/CommandLine.h"
#include "clang/Tooling/CompilationDatabase.h"

#include <filesystem>

using namespace clang::tooling;
namespace fs = std::filesystem;
namespace cl = llvm::cl;

// Main filename, compilation flags for which get outputted:
static cl::opt<std::string> file_name(cl::Positional, cl::Required, cl::desc("<input file>"));

// Options to output path to the compiler, e.g. print:
// /usr/bin/clang++ -I/my/includes my-file.cpp

// Instead of just: -I/my/includes my-file.cpp

static cl::opt<bool> print_compiler_path("output-compiler-path", cl::desc("Show compiler path"));
static cl::alias print_compiler_path_alias("f", cl::desc("Alias for --output-compiler-path"),
                                           cl::aliasopt(print_compiler_path));

// Recursively find compilation database for file 'current_path',
// look in current directory, all its parents and nested build directories
llvm::Optional<fs::path> find_compilation_database(fs::path current_path) {
    if (!fs::is_directory(current_path))
        return find_compilation_database(current_path.parent_path());

    auto supposed_path = current_path / "compile_commands.json";
    if (fs::exists(supposed_path))
        return supposed_path.parent_path();

    supposed_path = current_path / "build/compile_commands.json";
    if (fs::exists(supposed_path))
        return supposed_path.parent_path();

    if (current_path == "/")
        return llvm::None;

    return find_compilation_database(current_path.parent_path());
}
    
cl::extrahelp more_help("\n"
    "This program finds compilations database for selected file "
    "(it looks in file's directory, all its parents and all "
    "subdirectories called \"build\" in all of the latter). "

    "\n"
    "\n"

    "When it finds one, it outputs compilation flags for selected "
    "file, which lets you the run clang on it directly.\n");

int main(int argc, const char **argv) {
    cl::ParseCommandLineOptions(argc, argv, "Output compilation flags for selected file");

    fs::path file_path(file_name.getValue());
    auto db = find_compilation_database(fs::absolute(file_path)).value().string();

    std::string error_message;
    auto compilation_db = CompilationDatabase::loadFromDirectory(db, error_message);

    if (!compilation_db) {
        llvm::errs() << error_message << "\n";
        return EXIT_FAILURE;
    }

    auto build_commands = compilation_db->getCompileCommands(file_name);
    if (build_commands.size() != 1) {
        llvm::errs() << error_message << "\n";
        return EXIT_FAILURE;
    }

    auto selected_cli_command = build_commands[0].CommandLine;
    //                                         ^ suppose we only have one build command

    for (std::size_t i = !print_compiler_path; i < selected_cli_command.size(); ++ i)
        llvm::outs() << selected_cli_command[i]
                     << (i != selected_cli_command.size() - 1? " " : "");
    //                  ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ avoid extra spaces

    return 0;
}
