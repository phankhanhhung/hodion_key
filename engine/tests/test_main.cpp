#include <cstdio>

int g_failures = 0;
int g_checks = 0;

void run_telex_tests();
void run_vni_tests();
void run_engine_tests();
void run_edge_tests();
void run_fuzz_tests();

int main() {
  run_telex_tests();
  run_vni_tests();
  run_engine_tests();
  run_edge_tests();
  run_fuzz_tests();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
