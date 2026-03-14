#include "Database.hpp"

#include <iostream>
#include <print>
#include <string>
#include <vector>

#include "parsing_functions.hpp"

// ---------------------------------------------------------------------------
// Print usage hint for a specific command.
// ---------------------------------------------------------------------------
static void usage(const std::string &cmd) {
  if (cmd == "BEGIN")
    std::println("Usage: BEGIN <tid>");
  else if (cmd == "ADD")
    std::println("Usage: ADD <tid> <rel> <file>");
  else if (cmd == "DELETE")
    std::println("Usage: DELETE <tid> <rel> <file>");
  else if (cmd == "COMMIT")
    std::println("Usage: COMMIT <tid>");
  else if (cmd == "ROLLBACK")
    std::println("Usage: ROLLBACK <tid>");
  else if (cmd == "QUERY")
    std::println(
        "Usage: QUERY <tid> RelName(arg,arg) [, RelName(arg,arg) ...]");
}

// ---------------------------------------------------------------------------
// Main REPL
// ---------------------------------------------------------------------------
int main() {
  Database db;
  std::string line;

  // Only print a prompt when input comes from a terminal so that piped /
  // redirected test scripts produce clean output.
  const bool interactive = isatty(fileno(stdin));

  while (true) {
    if (interactive)
      std::print("> ");

    if (!std::getline(std::cin, line))
      break; // EOF

    // Strip leading/trailing whitespace and skip blank or comment lines.
    {
      size_t s = line.find_first_not_of(" \t\r");
      if (s == std::string::npos)
        continue;
      size_t e = line.find_last_not_of(" \t\r");
      line = line.substr(s, e - s + 1);
    }
    if (line.empty() || line[0] == '#')
      continue;

    std::vector<std::string> toks = tokenise(line);
    if (toks.empty())
      continue;

    const std::string &cmd = toks[0];

    if (cmd == "BEGIN") {
      // ------------------------------------------------------------------
      // BEGIN <tid>
      // ------------------------------------------------------------------
      if (toks.size() != 2) {
        usage(cmd);
        continue;
      }
      const std::string &tid = toks[1];
      if (!db.begin_transaction(tid)) {
        std::println("ERROR: transaction '{}' already exists", tid);
      } else {
        std::println("Transaction '{}' was created", tid);
      }

    } else if (cmd == "ADD") {
      // ------------------------------------------------------------------
      // ADD <tid> <rel> <file>
      // ------------------------------------------------------------------
      if (toks.size() != 4) {
        usage(cmd);
        continue;
      }
      db.add_data(toks[1], toks[2], toks[3]);

    } else if (cmd == "DELETE") {
      // ------------------------------------------------------------------
      // DELETE <tid> <rel> <file>
      // ------------------------------------------------------------------
      if (toks.size() != 4) {
        usage(cmd);
        continue;
      }
      db.delete_data(toks[1], toks[2], toks[3]);

    } else if (cmd == "COMMIT") {
      // ------------------------------------------------------------------
      // COMMIT <tid>
      // ------------------------------------------------------------------
      if (toks.size() != 2) {
        usage(cmd);
        continue;
      }
      const std::string &tid = toks[1];
      db.commit_transaction(tid);

    } else if (cmd == "ROLLBACK") {
      // ------------------------------------------------------------------
      // ROLLBACK <tid>
      // ------------------------------------------------------------------
      if (toks.size() != 2) {
        usage(cmd);
        continue;
      }
      const std::string &tid = toks[1];
      db.rollback_transaction(tid);
    } else if (cmd == "QUERY") {
      // ------------------------------------------------------------------
      // QUERY <tid> <body>
      // body ::= Atom (, Atom)*
      // Atom ::= RelName(arg, arg)
      // arg  ::= integer | variable_name
      // ------------------------------------------------------------------
      if (toks.size() < 3) {
        usage(cmd);
        continue;
      }
      const std::string &tid = toks[1];
      // Extract body: everything after "QUERY <tid> ".
      // Use token positions to be robust: skip past the second token.
      const std::size_t body_start = line.find(toks[1]) + toks[1].size();
      const std::size_t non_space = line.find_first_not_of(' ', body_start);
      std::string body = (non_space == std::string::npos)
                             ? std::string{}
                             : line.substr(non_space);

      std::string err;
      auto atoms = parse_query(body, err);
      if (!atoms) {
        std::println("ERROR: invalid query syntax — {}", err);
        continue;
      }
      db.query(tid, atoms.value());
    } else if (cmd == "RESUME") {
      // ------------------------------------------------------------------
      // RESUME <tid>
      // ------------------------------------------------------------------
      if (toks.size() != 2) {
        usage(cmd);
        continue;
      }
      const std::string &tid = toks[1];
      db.resume_transaction(tid);
    } else {
      // ------------------------------------------------------------------
      // Unknown command
      // ------------------------------------------------------------------
      std::println("ERROR: unknown command '{}'. Known commands: BEGIN ADD "
                   "DELETE COMMIT ROLLBACK QUERY RESUME",
                   cmd);
    }
  }

  if (interactive)
    std::println();
  return 0;
}
