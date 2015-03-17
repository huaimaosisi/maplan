from __future__ import print_function

import invariant_finder
import pddl
import timers


DEBUG = False


def expand_group(group, task, reachable_facts):
    result = []
    for fact in group:
        try:
            pos = list(fact.args).index("?X")
        except ValueError:
            result.append(fact)
        else:
            # NOTE: This could be optimized by only trying objects of the correct
            #       type, or by using a unifier which directly generates the
            #       applicable objects. It is not worth optimizing this at this stage,
            #       though.
            for obj in task.objects:
                newargs = list(fact.args)
                newargs[pos] = obj.name
                atom = pddl.Atom(fact.predicate, newargs)
                if atom in reachable_facts:
                    result.append(atom)
    task.mark_private_atoms(result)
    return result

def instantiate_groups(groups, task, reachable_facts):
    return [expand_group(group, task, reachable_facts) for group in groups]

class GroupCoverQueue:
    def __init__(self, groups, partial_encoding):
        self.partial_encoding = partial_encoding
        if groups:
            self.max_size = max([len(group) for group in groups])
            self.groups_by_size = [[] for i in range(self.max_size + 1)]
            self.groups_by_fact = {}
            for group in groups:
                group = set(group) # Copy group, as it will be modified.
                self.groups_by_size[len(group)].append(group)
                for fact in group:
                    self.groups_by_fact.setdefault(fact, []).append(group)
            self._update_top()
        else:
            self.max_size = 0
    def __bool__(self):
        return self.max_size > 1
    __nonzero__ = __bool__
    def pop(self):
        result = list(self.top) # Copy; this group will shrink further.
        if self.partial_encoding:
            for fact in result:
                for group in self.groups_by_fact[fact]:
                    group.remove(fact)
        self._update_top()
        return result
    def _update_top(self):
        while self.max_size > 1:
            max_list = self.groups_by_size[self.max_size]
            while max_list:
                candidate = max_list.pop()
                if len(candidate) == self.max_size:
                    self.top = candidate
                    return
                self.groups_by_size[len(candidate)].append(candidate)
            self.max_size -= 1

def choose_groups(groups, reachable_facts, partial_encoding=True):
    queue = GroupCoverQueue(groups, partial_encoding=partial_encoding)
    uncovered_facts = reachable_facts.copy()
    result = []
    while queue:
        group = queue.pop()
        uncovered_facts.difference_update(group)
        result.append(group)
    print(len(uncovered_facts), "uncovered facts")
    result += [[fact] for fact in uncovered_facts]
    return result

def build_translation_key(groups):
    group_keys = []
    for group in groups:
        group_key = [str(fact) for fact in group]
        if len(group) == 1:
            group_key.append(str(group[0].negate()))
        else:
            group_key.append("<none of those>")
        group_keys.append(group_key)
    return group_keys

def collect_all_mutex_groups(groups, atoms):
    # NOTE: This should be functionally identical to choose_groups
    # when partial_encoding is set to False. Maybe a future
    # refactoring could take that into account.
    all_groups = []
    uncovered_facts = atoms.copy()
    for group in groups:
        uncovered_facts.difference_update(group)
        all_groups.append(group)
    all_groups += [[fact] for fact in uncovered_facts]
    return all_groups

def sort_groups(groups):
    return sorted(sorted(group) for group in groups)

def public_groups(groups):
    res = []
    for g in groups:
        g = list(filter(lambda x: not x.is_private, g))
        if len(g) > 0:
            #g = ['{0}({1})'.format(x.predicate, ','.join(x.args)) for x in g]
            g = [(x.predicate, x.args) for x in g]
            res += [g]
    return res

def ma_instantiate_groups(comm, groups, invariants, task, atoms):
    # Send public mutex groups to all other agents
    task.mark_private_atoms(groups)
    comm.sendToAll(public_groups(groups))

    # Extend set of initial atoms by mutex groups received from other
    # agents
    init_atoms = set(task.init)
    for other_groups in comm.recvFromAll():
        for group in other_groups:
            if len(group) <= 1:
                continue

            group = [pddl.Atom(x[0], x[1]) for x in group]
            if all([x not in init_atoms for x in group]):
                init_atoms.add(group[0])

    # If we haven't received any additional atom, exit with the old mutex
    # groups
    if len(init_atoms) == len(task.init):
        return groups

    # Try to instantiate mutex groups from a newly constructed initial
    # state
    invariant_groups = invariant_finder.useful_groups(invariants, init_atoms)
    invariant_groups = list(invariant_groups)
    new_groups = instantiate_groups(invariant_groups, task, atoms)
    if len(new_groups) > len(groups):
        return new_groups
    return groups

def compute_groups(task, atoms, reachable_action_params, partial_encoding=True,
                   comm = None):
    invariants, groups = invariant_finder.get_groups(task, reachable_action_params)

    with timers.timing("Instantiating groups"):
        groups = instantiate_groups(groups, task, atoms)

    if comm is not None:
        # Try to instantiate another mutex groups that are based on initial
        # states of other agents
        groups = ma_instantiate_groups(comm, groups, invariants, task, atoms)

    # Sort here already to get deterministic mutex groups.
    groups = sort_groups(groups)
    # TODO: I think that collect_all_mutex_groups should do the same thing
    #       as choose_groups with partial_encoding=False, so these two should
    #       be unified.
    with timers.timing("Collecting mutex groups"):
        mutex_groups = collect_all_mutex_groups(groups, atoms)
    with timers.timing("Choosing groups", block=True):
        groups = choose_groups(groups, atoms, partial_encoding=partial_encoding)

    with timers.timing("Building translation key"):
        translation_key = build_translation_key(groups)

    if DEBUG:
        for group in groups:
            if len(group) >= 2:
                print("{%s}" % ", ".join(map(str, group)))

    return groups, mutex_groups, translation_key
