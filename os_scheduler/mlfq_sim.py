from dataclasses import dataclass, field
from collections import deque
from typing import List, Optional, Deque, Dict

# -----------------------------
# Data Model
# -----------------------------
@dataclass
class Process:
    pid: str
    arrival: int
    burst: int
    base_priority: int  # smaller number = higher priority
    ptype: str          # "RT", "INTERACTIVE", "BATCH"

    remaining: int = field(init=False)
    first_start: Optional[int] = field(default=None, init=False)
    completion: Optional[int] = field(default=None, init=False)
    waiting: int = field(default=0, init=False)

    # For movement/aging
    last_enqueued_time: int = field(default=0, init=False)
    queue_level: int = field(default=0, init=False)  # 0,1,2

    def __post_init__(self):
        self.remaining = self.burst


# -----------------------------
# Scheduler Simulation
# -----------------------------
class MultiLevelScheduler:
    def __init__(self, processes: List[Process], rr_quantum: int = 3, aging_threshold: int = 8):
        self.processes = sorted(processes, key=lambda p: (p.arrival, p.pid))
        self.rr_quantum = rr_quantum
        self.aging_threshold = aging_threshold

        self.time = 0
        self.cpu_busy = 0
        self.completed = 0

        # Queues
        self.q0: List[Process] = []       # Preemptive Priority (we will sort each tick)
        self.q1: Deque[Process] = deque() # Round Robin
        self.q2: Deque[Process] = deque() # FCFS

        self.running: Optional[Process] = None
        self.running_quantum_left = 0

        self.arrival_index = 0
        self.total = len(self.processes)

    def enqueue(self, p: Process, level: int):
        p.queue_level = level
        p.last_enqueued_time = self.time
        if level == 0:
            self.q0.append(p)
        elif level == 1:
            self.q1.append(p)
        else:
            self.q2.append(p)

    def add_new_arrivals(self):
        while self.arrival_index < self.total and self.processes[self.arrival_index].arrival == self.time:
            p = self.processes[self.arrival_index]
            # Initial queue by type
            if p.ptype == "RT":
                self.enqueue(p, 0)
            elif p.ptype == "INTERACTIVE":
                self.enqueue(p, 1)
            else:
                self.enqueue(p, 2)
            self.arrival_index += 1

    def apply_aging(self):
        # Promote from Q2 -> Q1, Q1 -> Q0 if waited too long
        # We'll check all in Q2 and Q1 (not Q0)
        promoted_q2 = deque()
        while self.q2:
            p = self.q2.popleft()
            waited = self.time - p.last_enqueued_time
            if waited >= self.aging_threshold:
                # promote to Q1
                self.enqueue(p, 1)
            else:
                promoted_q2.append(p)
        self.q2 = promoted_q2

        promoted_q1 = deque()
        while self.q1:
            p = self.q1.popleft()
            waited = self.time - p.last_enqueued_time
            if waited >= self.aging_threshold:
                # promote to Q0
                self.enqueue(p, 0)
            else:
                promoted_q1.append(p)
        self.q1 = promoted_q1

    def pick_next(self) -> Optional[Process]:
        # Highest priority queue first
        if self.q0:
            # preemptive priority: choose smallest base_priority
            self.q0.sort(key=lambda p: (p.base_priority, p.arrival, p.pid))
            return self.q0.pop(0)
        if self.q1:
            return self.q1.popleft()
        if self.q2:
            return self.q2.popleft()
        return None

    def preempt_if_needed(self):
        # If currently running from Q1/Q2 but Q0 has someone, preempt
        if self.running and self.running.queue_level != 0 and self.q0:
            # Put running back to its queue (front for fairness)
            self.running.last_enqueued_time = self.time
            if self.running.queue_level == 1:
                self.q1.appendleft(self.running)
            else:
                self.q2.appendleft(self.running)
            self.running = None
            self.running_quantum_left = 0

        # If running is in Q0, but a higher-priority (smaller number) arrives in Q0, preempt
        if self.running and self.running.queue_level == 0 and self.q0:
            self.q0.sort(key=lambda p: (p.base_priority, p.arrival, p.pid))
            best = self.q0[0]
            if best.base_priority < self.running.base_priority:
                # preempt running
                self.running.last_enqueued_time = self.time
                self.q0.append(self.running)
                self.running = None
                self.running_quantum_left = 0

    def tick_waiting_time(self):
        # Everyone in queues waits 1 unit
        for p in self.q0:
            p.waiting += 1
        for p in self.q1:
            p.waiting += 1
        for p in self.q2:
            p.waiting += 1

    def run(self):
        # Run until all processes complete
        while self.completed < self.total:
            self.add_new_arrivals()
            self.apply_aging()
            self.preempt_if_needed()

            # If nothing running, pick next
            if self.running is None:
                self.running = self.pick_next()
                if self.running:
                    if self.running.first_start is None:
                        self.running.first_start = self.time
                    # set quantum if RR queue
                    if self.running.queue_level == 1:
                        self.running_quantum_left = self.rr_quantum
                    else:
                        self.running_quantum_left = 10**9  # effectively infinite

            # Waiting time increments for those not running
            self.tick_waiting_time()

            # Execute 1 time unit
            if self.running:
                self.cpu_busy += 1
                self.running.remaining -= 1
                self.running_quantum_left -= 1

                # Finished?
                if self.running.remaining == 0:
                    self.running.completion = self.time + 1
                    self.completed += 1
                    self.running = None
                    self.running_quantum_left = 0
                else:
                    # RR quantum expired?
                    if self.running.queue_level == 1 and self.running_quantum_left == 0:
                        # demote to Q2 (batch) after full quantum use
                        p = self.running
                        self.running = None
                        self.enqueue(p, 2)
                        self.running_quantum_left = 0

            self.time += 1

        return self.report()

    def report(self) -> Dict:
        finish_time = self.time
        cpu_util = (self.cpu_busy / finish_time) * 100.0 if finish_time > 0 else 0.0
        throughput = self.total / finish_time if finish_time > 0 else 0.0

        rows = []
        avg_tat = 0.0
        avg_wt = 0.0
        avg_rt = 0.0

        for p in sorted(self.processes, key=lambda x: x.pid):
            tat = p.completion - p.arrival
            wt = p.waiting
            rt = p.first_start - p.arrival
            avg_tat += tat
            avg_wt += wt
            avg_rt += rt
            rows.append((p.pid, p.arrival, p.burst, p.base_priority, p.ptype, p.first_start, p.completion, rt, wt, tat))

        avg_tat /= self.total
        avg_wt /= self.total
        avg_rt /= self.total

        return {
            "finish_time": finish_time,
            "cpu_utilization": cpu_util,
            "throughput": throughput,
            "avg_turnaround": avg_tat,
            "avg_waiting": avg_wt,
            "avg_response": avg_rt,
            "table": rows
        }


# -----------------------------
# Example Input Dataset
# -----------------------------
def demo_processes():
    # pid, arrival, burst, priority, type
    return [
        Process("P1", 0, 7, 2, "RT"),
        Process("P2", 1, 6, 1, "RT"),
        Process("P3", 2, 8, 5, "INTERACTIVE"),
        Process("P4", 3, 5, 6, "INTERACTIVE"),
        Process("P5", 4, 10, 9, "BATCH"),
        Process("P6", 6, 4, 8, "BATCH"),
    ]


def main():
    procs = demo_processes()
    sim = MultiLevelScheduler(procs, rr_quantum=3, aging_threshold=8)
    rep = sim.run()

    print("\n===== MULTI-LEVEL CPU SCHEDULER REPORT =====")
    print(f"Finish time: {rep['finish_time']}")
    print(f"CPU Utilization (%): {rep['cpu_utilization']:.2f}")
    print(f"Throughput (proc/unit time): {rep['throughput']:.4f}")
    print(f"Average Turnaround Time: {rep['avg_turnaround']:.2f}")
    print(f"Average Waiting Time: {rep['avg_waiting']:.2f}")
    print(f"Average Response Time: {rep['avg_response']:.2f}")

    print("\nPID  Arr  Burst  Pri  Type         First  Comp  Resp  Wait  TAT")
    print("----------------------------------------------------------------")
    for row in rep["table"]:
        pid, arr, burst, pri, ptype, first, comp, rt, wt, tat = row
        print(f"{pid:<4}{arr:<5}{burst:<7}{pri:<5}{ptype:<13}{first:<7}{comp:<6}{rt:<6}{wt:<6}{tat:<5}")


if __name__ == "__main__":
    main()
