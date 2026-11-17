// Routing chooses only between existing restricted original interpreters.
// It neither rewrites the expression nor attaches the full core library.
export function usesApplicationInterpreter(input: string): boolean {
  return input.trimStart().startsWith('app.');
}
