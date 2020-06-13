#include <fstream>
#include <iostream>
#include <string>

std::string last_component(const std::string& path) {
  auto index = path.find_last_of('/');
  if (index == std::string::npos) {
    return path;
  }
  return path.substr(index + 1);
}

std::string variable_name(const std::string& resource_path) {
  auto variable = last_component(resource_path);
  if (std::isdigit(variable[0])) {
    variable = "__" + variable;
  }
  for (char& c : variable) {
    if (c == '.' || c == '-') c = '_';
  }
  return variable;
}

int main(int argc, char** argv) {
  if (argc < 1) {
    std::cerr << "Usage " << argv[0] << " generated_filename [resources...]\n";
    return 1;
  }
  std::string generated_filename = argv[1];
  std::ofstream header_file(generated_filename + ".h");
  std::ofstream implementation_file(generated_filename + ".cpp");

  for (int i = 2; i < argc; i++) {
    std::string resource_path = argv[i];
    std::ifstream resource_file(resource_path);
    if (!resource_file) {
      std::cerr << "Couldn't read " << resource_path << "\n";
      return 1;
    }
    std::string variable = variable_name(resource_path);
    if (variable.empty()) {
      std::cerr << "Couldn't determine variable name for " << resource_path
                << "\n";
      return 1;
    }
    header_file << "extern unsigned char " << variable << "[];\n"
                << "extern unsigned int " << variable << "_len;\n";

    implementation_file << "unsigned char " << variable << "[] = { ";

    bool first_char = true;
    size_t size = 0;
    while (true) {
      char c;
      if (!resource_file.get(c)) {
        break;
      }
      if (first_char) {
        first_char = false;
      } else {
        implementation_file << ", ";
      }
      implementation_file << std::hex << "0x"
                          << static_cast<unsigned int>(
                                 static_cast<unsigned char>(c));
      size++;
    }
    implementation_file << " };\n";
    implementation_file << "unsigned int " << variable << "_len = " << std::dec
                        << size << ";\n";
  }

  return 0;
}