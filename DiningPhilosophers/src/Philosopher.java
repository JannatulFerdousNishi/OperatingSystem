public class Philosopher extends Thread {

    private final int id;
    private final DiningServer server;

    public Philosopher(int id, DiningServer server) {
        this.id = id;
        this.server = server;
    }

    private void think() throws InterruptedException {
        System.out.println("Philosopher " + id + " is THINKING");
        Thread.sleep((long) (Math.random() * 2000));
    }

    private void eat() throws InterruptedException {
        Thread.sleep((long) (Math.random() * 2000));
    }

    @Override
    public void run() {
        try {
            while (true) {
                think();
                server.takeForks(id);
                eat();
                server.returnForks(id);
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
