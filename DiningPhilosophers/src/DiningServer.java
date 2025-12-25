public interface DiningServer {
    void takeForks(int philosopherNumber) throws InterruptedException;
    void returnForks(int philosopherNumber);
}
