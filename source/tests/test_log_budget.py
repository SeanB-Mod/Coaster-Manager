import ctypes, concurrent.futures
from pathlib import Path
D=ctypes.CDLL(str(Path(__file__).with_name('test_log_budget.dll')))
D.BudgetClaim.argtypes=[ctypes.c_uint];D.BudgetClaim.restype=ctypes.c_int
D.BudgetReset.argtypes=[ctypes.c_uint];D.BudgetBytes.restype=ctypes.c_uint
limit=8*1024*1024; data_limit=limit-128
D.BudgetReset(0);assert D.BudgetClaim(0)==0
assert D.BudgetClaim(data_limit)==1 and D.BudgetClaim(1)==0
D.BudgetReset(0);assert D.BudgetClaim(0xffffffff)==-1 and D.BudgetBytes()==limit
assert D.BudgetClaim(1)==0
D.BudgetReset(0)
def claims(_):
 results=[D.BudgetClaim(997) for _ in range(4000)]
 return results.count(1),results.count(-1)
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:r=list(pool.map(claims,range(8)))
assert sum(a for a,b in r)*997<=data_limit
assert sum(b for a,b in r)==1 and D.BudgetBytes()==limit
print('PASS: atomic log budget bounds exact/zero/oversized writes and 32,000 concurrent claims; only one truncation marker owner.')
