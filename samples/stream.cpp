// Bundle with mixed directions
struct Stream {
    ch_out<ch_uint<1>> valid;  // Producer: output
    ch_in<ch_uint<1>>  ready;  // Producer: input
    ch_uint<8>         data;
};

class Producer : public Component {
public:
    ch_out<Stream> io;  // All fields keep their direction
    Producer(Component* parent = nullptr) : Component(parent, "producer") {}
    void describe() override {
        io.valid = 1;
        io.data  = counter;
        // io.ready is input, can be used in expressions
    }
};

class Consumer : public Component {
public:
    ch_in<Stream> io;  // valid=in, ready=out
    Consumer(Component* parent = nullptr) : Component(parent, "consumer") {}
    void describe() override {
        // Use io.valid, io.data
        // Drive io.ready = 1;
    }
};

class Top : public Component {
public:
    Top(Component* parent = nullptr) : Component(parent, "top") {}
    void describe() override {
        Producer p;
        Consumer c;
        // Connect
        c.io.valid = p.io.valid;
        c.io.data  = p.io.data;
        p.io.ready = c.io.ready;
    }
};

int main() {
    // 直接构造顶层模块（无父）
    ch::ch_device<Top> top; // 语义清晰：这是顶层设备
    /*
    ch::Simulator sim(top.context());
    sim.run(10);
    */
    // 后续：Simulator sim(top.context()); ...
    return 0;
}
