#include <nxtcut/model/compound_graph.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_set>
#include <variant>
#include <vector>

namespace nxtcut::model {

namespace {

enum class VisitColor : std::uint8_t {
    White,
    Gray,
    Black,
};

struct StackFrame {
    SequenceId u;
    std::size_t next_child_idx{0};
};

}  // namespace

std::vector<std::vector<SequenceId>> find_compound_cycles(const Project& project) {
    // Build deterministic adjacency list for existing sequences, deduplicating parallel edges
    std::map<SequenceId, std::vector<SequenceId>> adj;
    for (const auto& [seq_id, seq] : project.sequences) {
        std::vector<SequenceId>& neighbors = adj[seq_id];
        std::unordered_set<SequenceId> seen_neighbors;
        for (const auto& track : seq.tracks) {
            for (const auto& clip : track.clips) {
                if (const auto* comp = std::get_if<CompoundContent>(&clip.content)) {
                    if (project.sequences.find(comp->sequence) != project.sequences.end()) {
                        if (seen_neighbors.insert(comp->sequence).second) {
                            neighbors.push_back(comp->sequence);
                        }
                    }
                }
            }
        }
    }

    std::map<SequenceId, VisitColor> colors;
    for (const auto& [seq_id, _] : project.sequences) {
        colors[seq_id] = VisitColor::White;
    }

    std::vector<std::vector<SequenceId>> cycles;

    // Iterative depth-first search in map key order
    for (const auto& [start_id, _] : project.sequences) {
        if (colors[start_id] != VisitColor::White) {
            continue;
        }

        std::vector<StackFrame> stack;
        colors[start_id] = VisitColor::Gray;
        stack.push_back(StackFrame{start_id, 0});

        while (!stack.empty()) {
            StackFrame& top = stack.back();
            const auto& neighbors = adj[top.u];

            if (top.next_child_idx < neighbors.size()) {
                const SequenceId v = neighbors[top.next_child_idx++];
                const VisitColor v_color = colors[v];

                if (v_color == VisitColor::Gray) {
                    // Back edge detected: extract cycle path from v up to top.u
                    std::size_t v_idx = 0;
                    for (std::size_t i = 0; i < stack.size(); ++i) {
                        if (stack[i].u == v) {
                            v_idx = i;
                            break;
                        }
                    }
                    std::vector<SequenceId> cycle;
                    for (std::size_t i = v_idx; i < stack.size(); ++i) {
                        cycle.push_back(stack[i].u);
                    }
                    cycles.push_back(std::move(cycle));
                } else if (v_color == VisitColor::White) {
                    colors[v] = VisitColor::Gray;
                    stack.push_back(StackFrame{v, 0});
                }
            } else {
                colors[top.u] = VisitColor::Black;
                stack.pop_back();
            }
        }
    }

    return cycles;
}

std::optional<std::vector<SequenceId>> find_compound_cycle(const Project& project) {
    const auto cycles = find_compound_cycles(project);
    if (cycles.empty()) {
        return std::nullopt;
    }
    return cycles.front();
}

bool would_create_cycle(const Project& project, SequenceId parent, SequenceId child) {
    if (child == parent) {
        return true;
    }

    // Iterative reachability search (using a stack) to test if parent is reachable from child
    std::vector<SequenceId> pending;
    std::unordered_set<SequenceId> visited;
    pending.push_back(child);
    visited.insert(child);

    while (!pending.empty()) {
        const SequenceId curr = pending.back();
        pending.pop_back();

        const auto* seq = find_sequence(project, curr);
        if (seq == nullptr) {
            continue;
        }

        for (const auto& track : seq->tracks) {
            for (const auto& clip : track.clips) {
                if (const auto* comp = std::get_if<CompoundContent>(&clip.content)) {
                    if (comp->sequence == parent) {
                        return true;
                    }
                    if (visited.insert(comp->sequence).second) {
                        pending.push_back(comp->sequence);
                    }
                }
            }
        }
    }

    return false;
}

}  // namespace nxtcut::model
