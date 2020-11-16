#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <dirent.h>
#include <queue>
#include <thread>
#include <mutex>
#include <stdlib.h>

struct node // состояния КМП-автомата
{
    node* son;
    node* parent;
    char move_son;
    bool is_terminal;
    node* ex_link;

    node (node* parent ) {
        this->parent = parent;
        son = nullptr;
        is_terminal = false;
    }

    ~node() {
        delete son;
    }
};



struct KMP // сам автомат
{
    node* root;

    KMP () {
        root = new node(nullptr);
    }

    void build_dictionary (const std::string &token) {
        node* current_pos = root;
        for ( auto ch : token ) {
            current_pos->move_son = ch;
            current_pos->son = new node ( current_pos);
            current_pos = current_pos->son;
        }
        current_pos->is_terminal = true;
    }

    void get_ex_link_for_pos (node* current_pos) {
        node* possible_ex_link = current_pos->parent->ex_link;
        while ( possible_ex_link != root ) {
            if (possible_ex_link->move_son == current_pos->parent->move_son ) {
                current_pos->ex_link = possible_ex_link->son;
                return;
            }
            else {
                possible_ex_link = possible_ex_link->parent;
            }
        }
        if ( possible_ex_link->move_son == current_pos->parent->move_son )
            current_pos->ex_link = root->son;
        else
            current_pos->ex_link = root;
    }

    void build_ex_link () {
        root->ex_link = root;
        if ( root->son != nullptr )
            root->son->ex_link = root;
        node* current_pos = root->son->son;
        while ( current_pos != nullptr ) {
            get_ex_link_for_pos(current_pos);
            current_pos = current_pos->son;
        }
    }

    void build (const std::string token) {
        this->build_dictionary(token);
        this->build_ex_link();
    }


    ~ KMP () {
        delete root;

    }
};


bool not_only_slashes_dots ( std::string  s) { // для удаления "побочных" по типу ./. пояляющихся при обходе дирректории 
  unsigned int idx = 0;
  while ( idx < s.size() ) {
    switch ( s[idx] ) {
      case '.': ++idx ; break;
      case '/': ++idx; break;
      default : return true;
    }
  }
  return false;
}


void ls( std::string const &name, bool flag ,std::queue<std::string> &ret) { // обход дирректории
  DIR *dir = opendir(name.c_str());
  if (dir == nullptr) return;
  for (dirent *d = readdir(dir); d != nullptr; d = readdir(dir)) {
    if ( d->d_type == DT_DIR) {
      if (strcmp(d->d_name, ".") == 0) continue;
      if (strcmp(d->d_name,"..") == 0) continue;
      if ( flag ) // надо ли заходить в поддиректории
        ls(name + std::string("/") + d->d_name, flag ,ret);
    } else {
        if ( not_only_slashes_dots( (std::string)d->d_name) ) {
          std::string temp = name + "/" + d->d_name;
          ret.push( temp ) ;
        }
    }
  }
  closedir(dir);
}



KMP avt; // будующий автомат
std::string token; // образец
std::string path; // путь поиска
bool flag; //нужны ли поддиректории
bool finish_build_queue ;
int kol_treades; // кол-во нитей
std::queue< std::string > Queue_file; // очередь из возможных файлов 

std::mutex queue_guard; // mutex на очередь
std::mutex cout_guard; // mutex на вывод

bool contain ( const std::string &line, const std::string &token ) { // проверка отдельной строки
    node* pos = avt.root;
        for (auto it : line ) { // работа КМП автомата
            if ( pos->is_terminal )
                return true;
            if (pos->move_son == it )
                pos = pos->son;
            else {
                bool success_move = false;
                pos = pos->ex_link;
                while ( pos != avt.root ) {
                    if ( pos->move_son == it ) {
                        pos = pos->son;
                        success_move = true;
                        break;
                    }
                }
                if ( !success_move ) {
                    if ( pos->move_son = it)
                        pos = pos->son;
                    else
                        pos = avt.root;
                }
            }
        }
        return pos->is_terminal;
}

 
void searcher(const std::string& w,  std::istream& input, std::string filename){ // построчная проверка файла
    std::string line;
    size_t k = 1;
    while (std::getline(input, line))
    {
        if (  contain(line, w) ) {
            cout_guard.lock();
            std::cout<<filename<<" : "<< k <<" : "<<line<<'\n';
            cout_guard.unlock();
         }
        ++k;
    }
}

void walker () { // распаралеленная функция работа потока
    queue_guard.lock();
    if ( Queue_file.empty() ) { 
        queue_guard.unlock();
        if ( finish_build_queue ) // очередь закончилась
            return;
        else
            walker();
    }
    else{
        std::string path = Queue_file.front();
        Queue_file.pop();
        queue_guard.unlock();
        std::ifstream file;
        file.open(path); //Попытка открытия файла
        if (! file.is_open() ) { // в случае неудачи начать заново
            walker();
            return;
        }
        else {
            searcher(token, file, path);
            walker(); // вернутсья к очереди за новым файлом
            return;
        }
    }
}

void parser (std::vector<std::string> &A ) { // парсер переданных аргументов
    bool flag_double_t = false;
    int kol_read = 0;
    for ( auto it : A) {
        if ( it[0] == '-') {
            if ( it.size() == 1 ) {
                std::cout<<"нет флага после -"<<'\n';
                exit(0);
            }
            if ( it[1] == 'n' ) 
                flag = false;
            else {
                if (it[1] == 't' && !flag_double_t ) {
                    std::string namber;
                    if ( it.size() == 2 ) {
                        std::cout<<"не указано число потоков после флага"<<'\n';
                        exit(0);
                    }
                    for (int i = 2; i < it.size(); ++i )
                        namber += it[i];
                    kol_treades = atoi(namber.c_str());        
                }
                else {
                    std::cout<<"дважды указано число потоков"<<'\n';
                    exit(0);
                }
            }
        }
        else {
            if ( kol_read == 2) {
                std::cout<<"дважды указана директория"<<'\n';
                exit(0);
            }
            if ( kol_read == 0 )
                token = it;
            if ( kol_read == 1 )
                path = it;
            ++kol_read;
        }
    }
}


int main(int argc, char* argv[]) {

    path = ".";
    flag = true;  // значения по умолчанию
    kol_treades = 1;
    finish_build_queue = false;

    if ( argc == 1 ) {
        std::cout<<"не переданно никаких аргументов"<<'\n';
        exit(0);
    }

    std::vector<std::string> A(argc-1);
    for (int i = 0; i < argc; ++i ){
        if ( i == 0 )
            continue;
        else
            A[i-1] = argv[i];
    }
    parser(A);
    avt.build(token);
    std::vector<std::thread> searcher_threads;
    for ( int i = 0; i < kol_treades-1; ++i)
        searcher_threads.push_back( std::thread(walker) );
    ls(path, flag, Queue_file);
    finish_build_queue = true;
    walker();
    if ( kol_treades > 1){
        for ( int i = 0; i < kol_treades-1; ++i)
            searcher_threads[i].join(); 
    }
    return 0;
}
