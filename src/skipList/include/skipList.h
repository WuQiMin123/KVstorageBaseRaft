#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>

#define STORE_FILE "store/dumpFile"

static std::string delimiter = ":";     //用于分隔 key 和 value 在文件中的存储格式

template <typename K, typename V>
class Node{
public:
    Node(){}
    
    Node(K k, V v, int);

    ~Node();

    K get_key() const;
    V get_value() const;

    void set_value(V);

    Node<K, V> **forward;   //用来存储不同层级上的下一个节点的指针

    int node_level;
private:
    K key;
    V value;
};

template <typename K, typename V>
Node<K, V>::Node(const K k, const V v, int level){
    // 成员变量初始化
    this->key = k;
    this->value = v;
    this->node_level = level;

    this->forward = new Node<K, V> *[level + 1];    //动态分配一个指针数组 forward，其大小为 level + 1

    memset(this->forward, 0, sizeof(Node<K, V> *) * (level + 1));
};

template<typename K, typename V>
Node<K, V>::~Node(){
    delete[] forward;
};

template<typename K, typename V>
K Node<K, V>::get_key() const {
    return key;
};

template<typename K, typename V>
V Node<K, V>::get_value() const{
    return value;
};

template<typename K, typename V>
V Node<K, V>::set_value(V value){
    this->value = value;
};




template <typename K, typename V>
class SkipListDump{
public:
    friend class boost::serialization::access;

    template <class Archive>    // 什么意思？
    void serialize(Archive &ar, const unsigned int version){    // 这里将 keyDumpVt_ 和 valDumpVt_ 向量序列化
        ar &keyDumpVt_;     //& 是重载运算符，用于“存入”或“取出”数据。
        ar &valDumpVt_;
    }

    void insert(const Node<K, V> &node);

    //两个向量一一对应，用于重建跳表结构
    std::vector<K> keyDumpVt_;  //存储跳表中所有节点的键。
    std::vector<V> valDumpVt_;  
};

template<typename K, typename V>
class SkipList{
public:
    SkipList(int);
    ~SkipList();

    int get_random_level();
    Node<K, V> *create_node(K, V, int);
    void display_list();
    
    int insert_element(K, V);
    void insert_set_element(K &, V &);  //支持引用参数插入
    bool search_element(K, V &value);
    void delete_element(K);


    //持久化支持
    std::string dump_file();
    void load_file(const std::string &dumpStr);

    //递归删除节点
    void clear(Node<K, V> *);
    int size();

private:
    void get_key_value_from_string(const std::string &str, std::string *key, std::string *value);   //解析 key:value 格式的字符串
    bool is_valid_string(const std::string & str);  //检查字符串是否符合格式

private:
    int _max_level;
    int _skip_list_level;
    Node<K, V> *_header;    //辅助节点

    // std::ofstream _file_writer;
    // std::ifstream _file_reader;

    int _element_count;

    std::mutex _mtx;
};

template<typename K, typename V>
Node<K, V> *SkipList<K, V>::create_node(const K k, const V v, int level){
    Node<K, V> *n = new Node<K, V>(k, v, level);
    return n;
}

//在跳表中按层级查找，找出每一层中“小于目标 key 的最后一个节点”
// L3: 1 ---------------------> 10 --------------------------> 20
// L2: 1 --------> 7 ---------> 10 --------> 13 ------> 16 ----> 20
// L1: 1 --> 3 --> 5 --> 7 --> 10 --> 12 --> 13 --> 14 --> 16 --> 20
// L0: 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20
// 对于节点 10 ：forward[0]指向11; forward[1]指向12; forward[2]指向13...
// 对于节点 7 : forward[0]指向8; forward[1]指向10; forward[3]=nullptr
// 插入 key = 15 update[3] = 10; update[2] = 13; update[1] = 14; update[0] = 14

template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value){
    _mtx.lock();

    // 0.准备
    Node<K, V> *current = this->_header;
    Node<K, V> *update[_max_level + 1];
    memset(update, 0, sizeof(Node<K, V> *) * (_max_level + 1));

    // 1.找到要插入的位置
    for(int i = _skip_list_level; i >= 0; i--){
        while(current->forward[i] != NULL && current->forward[i]->get_key < key){
            current = current->forward[i];      //forward[i]：表示第 i 层上当前节点的下一个节点的指针。
        }
        update[i] = current; //存了每一层中要插入位置的前一个节点
    }
    current = current->forward[0];
    
    // 2.key已经存在的情况
    if(current != NULL && current->get_key() == key){
        std::cout << "Key: " << key << ", exists" << std:::endl;
        _mtx.unlock();
        return 1;
    }

    // 3.插入节点
    if(current == NULL || current->get_key() != key){
        // 3.1如果随机级别大于跳表的当前级别，则用指向头节点的指针初始化更新值。（这部分不明白）
        int random_level = get_random_level();
        if(random_level > _skip_list_level){
            for(int i = _skip_list_level + 1; i < random_level + 1; i++){
                update[i] = _header;
            }
            _skip_list_level = random_level;
        }

        // 3.2创建节点
        Node<K, V> *inserted_node = create_node(key, value, random_level);

        // 3.3插入节点
        for(int i = 0; i <= random_level; i++){
            inserted_node->forward[i] = update[i]->forward[i];  //移交下一个节点的指针
            update[i]->forward[i] = inserted_node;      // 更新继承来指向下一节点的指针
        }
        std::cout << "Successfully inserted key:" << key << ", value:" << value << std::endl;
        _element_count++;
    }
    _mtx.unclock();
    return 0;
}

template<typename K, typename V>
void SkipList<K, V>::display_list(){
    std::cout << "\n*****Skip List*****" << endl;
    for(int i = 0; i <= _skip_list_level; i++){
        Node<K, V> *node = this->_header->forward[i];
        std::cout << "Level " << i << ": ";
        while(node != NULL){
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

template<typename K, typename V>
std::string SkipList<K, V>::dump_file(){
    Node<K, V> *node = this->_header->forward[0];
    SkipListDump<K, V> dumper;  //收集和序列化跳表中的数据
    while(node != nullptr){
        dumper.insert(*node);   //插入到 dumper 对象中
        node = node->forward[0];
    }
    std::stringstream ss;   //存储序列化后的文本数据
    boost::archive::text_oarchive oa(ss); //一个用于将数据序列化为文本格式的类
    oa << dumper;   // 将 dumper 对象序列化到 oa 中, oa 相当于一个中介,用于序列化
    // ss.str() << ss << oa << dumper
    return ss.str();
}

template<typename K, typename V>
void SkipList<K, V>::load_file(const std::string &dumpStr){
    if(dumpStr.empty()){
        return ;
    }
    SkipListDump<K, V> dumper; 
    std::stringstream iss(dumpStr); //将字符串 dumpStr 包装成一个字符串流 iss
    boost::archive::text_iarchive ia(iss);      //创建一个 Boost 的文本输入归档对象 ia，它可以从流中读取文本格式的序列化数据
    ia >> dumper;
    // dumpStr >> iss >> ia >> dumper
    // 新构建跳表
    for(int i = 0; i < dumper.keyDumpVt_.size(); i++){
        insert_element(dumper.keyDumpVt_[i], dumper.valDumpVt_[i]); 
    }
}

template<typename K, typename V>
int SkipList<K, V>::size(){
    return _element_count;
}

template <typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string &str. std::string *key, std::string *value){
    if(!is_valid_string(str)){
        return ;
    }
    *key = str.substr(0, str.find(delimiter));
    *value = str.substr(str.find(delimiter) + 1, str.length());
}

template<typename K, typename V>
void SkipList<K, V>::isvalid_string(const std::string &str){
    if(str.empty()){
        return false;
    }
    if(str.find(delimiter) == std::string::npos){
        return false;
    }
    return true;
}

template<typename K, typename V>
void SkipList<K, V>::delete_element(K key){
    _mtx.lock();
    
    // 0.准备
    Node<K, V> *current = this->_header;
    Node<K, V> *update[_max_level + 1];
    memset(update, 0, sizeof(Node<K, V> *) * (_max_level + 1));

    // 1.记录目标节点的前置节点
    for(int i = _skip_list_level; i >= 0; i--){
        while(current != NULL && current->forward[i]->get_key() < key){
            current = current->forward[i];
        }
        update[i] = current;    
    }

    // 2.删除过程
    current = current->forward[0];
    if(current != NULL && current->get_key() == key){
        // 删除节点
        for(int i = 0; i <= _skip_list_level; i++){
            if(update[i]->forward[i] != current)    break;  // 如果在第i层current不存在（前一个指针不是指向current）
            update[i]->forward[i] = current->forward[i];
        }

        // 删除没有节点的层
        while(_skip_list_level > 0 && _header->forward[_skip_list_level] == 0){
            _skip_list_level--;
        }

        // 后续处理
        std::cout << "Successfully deleted key: " << key << std::endl;
        delete current;
        _element_count--;
    }

    _mtx.unclock();
    return ;
}

// 插入元素，如果元素存在则改变其值
template <typename K, typename V>
void SkipList<K, V>::insert_set_element(K &key, V &value){
    V oldValue;
    if(search_element(key, oldValue)){      // oldValue应该是一个空值，为什么可以引用？ 因为search_element会把找到的value装入&value
        delete_element(key);
    }
    insert_element(key, value);
}

template<typename K, typename V>
bool SkipList<K, V>::search_element(K key, V &value){
    std::cout << "\n*****search_element*****" << endl;
    Node<K, V> *current = _header;

    for(int i = _skip_list_level; i >= 0; i--){
        while(current->forward[i] != NULL && current->forward[i]->get_key() < key){
            current = current->forward[i];
        }
    }

    current = current->forward[0];

    if(current && current->get_key() == key){
        value = current->get_value();
        std::cout << "Found key: " << key << " Value: " << current->get_value() << std::endl;
        return true;
    }

    std::cout << "Not Found Key: " << key << endl;
    return false;
}

void SkipListDump<K, V>::insert(const Node<K, V>& ndoe){
    ketDumpVt_.emplace_back(node.get_key());
    valDumpVt_.emplace_back(node.get_value());
}

template<typename K, typename V>
SkipList<K, V>::SkipList(int max_value){
    this->_max_level = max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;

    K k;
    V v;
    this->_header = new Node<K, V>(k, v, _max_level);
}

template<typename K, typename V>
SkipList<K, V>::~SkipList(){
    if(_file_writer.is_open()){
        _file_writer.close();
    }
    if(_file_reader.is_open()){
        _file_reader.close();
    }

    // 递归删除跳表
    if(_header->forward[0] != nullptr){
        clear(cur->forward[0]);
    }
    delete(_header);
}

void SkipList<K, V>::clear(Node<K, V> *cur){
    if(cur->forward[0] != nullptr){
        clear(cur->forward[0]);
    }
    delete(cur);
}

template<typename K, typename V>
int SkipList<K, V>::get_random_level(){
    int k = 1;
    while(rand() % 2){      // 以概率方式模拟跳表的多级结构，每上升一个层级的概率是前一层的一半，优化了整个数据结构的访问效率
        k++;
    }
    k = (k < _max_level) ? k : _max_level;
    return k;
}


#endif