public class Main {
    public static void main(String[] args) {
        DiningServer server = new DiningServerImpl();
        Philosopher[] philosophers = new Philosopher[5];

        for (int i = 0; i < 5; i++) {
            philosophers[i] = new Philosopher(i, server);
            philosophers[i].start();
        }
    }
}
