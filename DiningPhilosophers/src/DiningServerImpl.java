import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class DiningServerImpl implements DiningServer {

    private static final int N = 5;

    private enum State { THINKING, HUNGRY, EATING }

    private final State[] state = new State[N];
    private final Lock lock = new ReentrantLock();
    private final Condition[] self = new Condition[N];

    public DiningServerImpl() {
        for (int i = 0; i < N; i++) {
            state[i] = State.THINKING;
            self[i] = lock.newCondition();
        }
    }

    private int left(int i) { return (i + N - 1) % N; }
    private int right(int i) { return (i + 1) % N; }

    private void test(int i) {
        if (state[i] == State.HUNGRY &&
                state[left(i)] != State.EATING &&
                state[right(i)] != State.EATING) {
            state[i] = State.EATING;
            self[i].signal();
        }
    }

    @Override
    public void takeForks(int i) throws InterruptedException {
        lock.lock();
        try {
            state[i] = State.HUNGRY;
            System.out.println("Philosopher " + i + " is HUNGRY");

            test(i);

            while (state[i] != State.EATING) {
                self[i].await();
            }

            System.out.println("Philosopher " + i + " starts EATING");
        } finally {
            lock.unlock();
        }
    }

    @Override
    public void returnForks(int i) {
        lock.lock();
        try {
            state[i] = State.THINKING;
            System.out.println("Philosopher " + i + " stops EATING and starts THINKING");

            test(left(i));
            test(right(i));
        } finally {
            lock.unlock();
        }
    }
}
