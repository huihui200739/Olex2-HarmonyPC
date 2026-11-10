"""Same ten genuine initialized-owner startup gates, only for exact candidate15."""
from texture_load_tool_profiles import startup_verifier_source
_namespace=dict(__name__='candidate15_exact_initialized_startup_verifier')
exec(compile(startup_verifier_source(),'candidate15_exact_initialized_startup_verifier','exec'),_namespace)
validate_runtime_snapshot=_namespace['validate_runtime_snapshot']
