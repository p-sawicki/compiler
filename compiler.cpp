#include "parser.h"

int main(int argc, char **argv) {
  char *inputFile = nullptr, *outputFile = nullptr;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp("-h", argv[i]) || !strcmp("--help", argv[i])) {
      std::cout << "Usage:\n"
                << argv[0]
                << " [INPUT_FILE] [(--output | -o) OUTPUT_FILE]\n"
                   "Give no input file to read from standard input.\n"
                   "Give no output file to write to standard output.\n";
      return 0;
    }
    if (!strcmp("-o", argv[i]) || !strcmp("--output", argv[i])) {
      ++i;
      if (i >= argc) {
        std::cerr << "Missing output file after " << argv[i - 1] << "\n";
        return 1;
      }
      if (outputFile) {
        std::cerr << "More than one output file\n";
        return 1;
      }
      outputFile = argv[i];
    } else {
      if (inputFile) {
        std::cerr << "More than one input file\n";
        return 1;
      }
      inputFile = argv[i];
    }
  }
  std::ifstream inputStream(inputFile);
  std::ifstream &in = inputStream;

  try {
    Lexer lexer = Lexer(inputFile ? in : std::cin);
    Parser parser(lexer);
    parser.parse();
  } catch (ParserError &err) {
    std::cerr << err.what() << "\nCompilation failed!\n";
    return 1;
  }

  std::error_code EC;
  llvm::raw_fd_ostream out(outputFile, EC);
  Node::module->print(outputFile ? out : llvm::outs(), nullptr);
}