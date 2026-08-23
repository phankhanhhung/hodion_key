#include <cstdio>

int g_failures = 0;
int g_checks = 0;

void run_telex_tests();
void run_vni_tests();
void run_engine_tests();

int main() {
  run_telex_tests();
  run_vni_tests();
  run_engine_tests();

  std::printf("%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
