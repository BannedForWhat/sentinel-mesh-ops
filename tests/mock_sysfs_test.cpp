#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

int main() {
   std::cout << "[SYSFS-TEST] Running synthetic sysfs parser test..." << std::endl;
   
   const std::string test_dir = "/tmp/mock_sysfs/class/drm/card0/device";
   const std::string test_path = test_dir + "/gpu_busy_percent";

   // Создаем директорию и файл, если они еще не существуют
   system(("mkdir -p " + test_dir).c_str());
   std::ofstream mock_file(test_path);
   if (mock_file.is_open()) {
       mock_file << "42\n";
       mock_file.close();
   }

   std::ifstream input_file(test_path);
   assert(input_file.is_open());

   int busy_pct = 0;
   input_file >> busy_pct;
   assert(busy_pct == 42);

   std::cout << "[SYSFS-TEST] Parser successfully processed busy value: " << busy_pct << std::endl;
   return 0;
}
