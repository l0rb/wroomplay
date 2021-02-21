struct Point
{
    int x;
    int y;
    static void (*cb)(int, int, bool);

    Point(int x_, int y_)
    {
        x = x_;
        y = y_;
    }
    void on()
    {
        cb(x, y, 1);
    }
    void off()
    {
        cb(x, y, 0);
    }

    bool operator < (const Point &other) const
    {
        if( y == other.y )
        {
            return x < other.x;
        }
        return y < other.y;
    }
    bool operator == (const Point &other) const
    {
        return y == other.y && x == other.x;
    }
};
void (*Point::cb)(int, int, bool);

class SnakeGame
{
    public:
        char direction = 'R';
        void step();
        SnakeGame(void (*sp)(int,int,bool), int w, int h) {
            Point::cb = sp;
            display_w = w;
            display_h = h;
            reset();
        }

        void input(char signal)
        {
            if( signal == 'D' && direction != 'U' )
                direction = 'D';
            if( signal == 'U' && direction != 'D' )
                direction = 'U';
            if( signal == 'L' && direction != 'R' )
                direction = 'L';
            if( signal == 'R' && direction != 'L')
                direction = 'R';
        }

    private:
        const static int snake_len_begin = 5;
        int snake_len = SnakeGame::snake_len_begin;
        std::queue<Point> snake;
        std::set<Point> not_snake;
        Point food = Point(0,0);
        int display_w;
        int display_h;

        bool add(Point p)
        {
            bool snake_ok = 0;
            if(not_snake.count(p))
            {
                p.on();
                snake.push(p);
                not_snake.erase(p);
                snake_ok = 1;
            }
            return snake_ok;
        }
        void remove_tail()
        {
            Point tail = snake.front();
            tail.off();
            not_snake.insert(tail);
            snake.pop();
        }
        Point head()
        {
            return snake.back();
        }

        void place_food()
        {
            auto it = std::begin(not_snake);
            auto r = rand() % not_snake.size(); 
            std::advance(it, r);
            food = (*it);
            Point::cb(food.x, food.y, 1);
        }
        bool move_snake()
        {
            Point new_head = head();
            if( direction == 'D' )
            {
                new_head.y += 1;
                new_head.y %= display_h;
            }
            else if( direction == 'U' )
            {
                new_head.y += (display_h-1);
                new_head.y %= display_h;
            }
            else if( direction == 'R' )
            {
                new_head.x += 1;
                new_head.x %= display_w;
            }
            else if( direction == 'L' )
            {
                new_head.x += (display_w - 1);
                new_head.x %= display_w;
            }
            return add(new_head);
        }
        void _step()
        {
            bool snake_ok = move_snake();
            if(!snake_ok)
            {
                reset();
            }
            else if(head() == food)
            {
                snake_len += 1;
                place_food();
            }
            while( snake.size() > snake_len )
            {
                remove_tail();
            }
        }
        void reset()
        {
            food.off();
            while(!snake.empty())
                remove_tail();
            not_snake.clear();

            for(int y_=0; y_<display_h; y_++)
            {
                for(int x_=0; x_<display_w; x_++)
                {
                    not_snake.insert(Point(x_,y_));
                }
            }

            snake_len = snake_len_begin;
            direction = 'R';
            add(Point(0,0));
            while(snake.size() < snake_len)
            {
                step();
            }
            place_food();
        }
};
void SnakeGame::step() { _step(); }

