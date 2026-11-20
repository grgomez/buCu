#include "cli/confirm.hpp"

#include <cstddef>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "bucu/platform/terminal.hpp"
#include "cli/exit_codes.hpp"
#include "cli/messages.hpp"

namespace bucu::cli {

namespace {

// Whether `answer` is a yes: y or yes in any case, ignoring spaces around it. Only ASCII is
// folded, so the answer does not depend on the locale.
bool is_yes(std::string_view answer) {
    constexpr std::string_view kSpace = " \t\n\r\f\v";
    const std::size_t first = answer.find_first_not_of(kSpace);
    if (first == std::string_view::npos) {
        return false;
    }
    answer = answer.substr(first, answer.find_last_not_of(kSpace) - first + 1);
    std::string lowered(answer);
    for (char& c : lowered) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return lowered == "y" || lowered == "yes";
}

}  // namespace

std::optional<int> check_can_confirm(const Context& context, bool yes, std::string_view needs) {
    if (yes || platform::is_interactive(context.terminals)) {
        return std::nullopt;
    }
    std::ostream& err = context.err;
    err << usage_error_message(std::string(needs) + " needs confirmation; add --yes to confirm")
        << '\n';
    return kExitUsage;
}

Answer ask(const Context& context, bool yes, std::string_view prompt) {
    if (yes) {
        return Answer::kProceed;
    }
    std::ostream& out = context.out;
    out << prompt << std::flush;
    std::string answer;
    if (!std::getline(context.input.get(), answer)) {
        // End of input: no Enter ended the prompt line.
        out << '\n';
    } else if (is_yes(answer)) {
        return Answer::kProceed;
    }
    out << "Cancelled.\n";
    return Answer::kCancelled;
}

}  // namespace bucu::cli
