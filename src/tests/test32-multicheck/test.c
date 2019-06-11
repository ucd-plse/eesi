int mustcheck();

int main() {
    int f = mustcheck();
    int b = mustcheck();

    if (!f || !b) {
        return 1; 
    }

    return 0;
}
