#include "../coaster_selection_probe.c"
__declspec(dllexport) void BudgetReset(unsigned n){log_budget_bytes=(long)n;}
__declspec(dllexport) int BudgetClaim(unsigned n){return log_budget_reserve(n);}
__declspec(dllexport) unsigned BudgetBytes(void){return (unsigned)log_budget_bytes;}
