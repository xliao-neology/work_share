// The obligatory "Hello World!" example.

#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/json_reader.hpp"
#include "caf/json_writer.hpp"

#include <iostream>
#include <string>

using namespace caf;
using namespace std::literals;
using namespace std;

// constexpr std::string_view example_input = R"([
//   {
//     "id": 1,
//     "name": "John Doe"
//   },
//   {
//     "id": 2,
//     "name": "Jane Doe",
//     "email": "jane@doe.com"
//   }
// ])";

struct user {
  uint32_t id;
  std::string name;
  std::string email;
  std::string payload;
  //std::optional<std::string> email;
  user(uint32_t ID=0, string Name="", string Email="", string Payload=""):id(ID),name(Name),email(Email), payload(Payload){}
  //user(uint32_t ID, string Name, string Email):id(ID),name(Name),email(Email){}
  user(const user&) = default;
  user& operator=(const user&) = default;
};

struct result_j {
  uint32_t a;
  uint32_t b;
  string c;
  vector<int> d;
  // result(uint32_t a_=0, uint32_t b_=0):a(a_),b(b_){}  
  // result(const result&) = default;
  // result& operator=(const result&) = default;
};


CAF_BEGIN_TYPE_ID_BLOCK(hello_world, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(hello_world, (user))
  CAF_ADD_TYPE_ID(hello_world, (result_j))

CAF_END_TYPE_ID_BLOCK(hello_world)

template <class Inspector>
bool inspect(Inspector& f, user& x) {
  return f.object(x).fields(f.field("id", x.id), f.field("name", x.name),
                            f.field("email", x.email), f.field("payload", x.payload));
}

template <class Inspector>
bool inspect(Inspector& f, struct result_j& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b), f.field("c", x.c), f.field("d", x.d));
}

behavior mirror(event_based_actor* self) {
  // return the (initial) actor behavior
  return {
    // a handler for messages containing a single string
    // that replies with a string
    [self](const user& what){
      //struct user jane=what;            
      struct result_j payload;
      aout(self).println(" \"id\": {}, \"name\": {}, \"email\": {}", what.id, what.name, what.email);
      caf::json_reader reader;      
      
      if(!reader.load(what.payload)){
        aout(self).println("error with loading json");
         std::cerr << "failed to parse JSON input: "
            << to_string(reader.get_error()) << '\n';
          throw std::logic_error("failed to parse JSON");
      }
      if(reader.apply(payload)){
        aout(self).println("parse json: {}, {}, {}", payload.a, payload.b, payload.c); //?
        for(auto& i:payload.d){
          aout(self).println("in vector:{}", i);
        }
      }else{
        aout(self).println("failed to parse json: ",  reader.get_error());
      }
    },
    [self](const std::string& what) -> std::string {
      // prints "Hello World!" via aout (thread-safe cout wrapper)
      aout(self).println("{}", what);
      // reply "!dlroW olleH"
      return std::string{what.rbegin(), what.rend()};
    },
  };
}

void hello_world(event_based_actor* self, const actor& buddy) {
  // send "Hello World!" to our buddy ...
  // self->request(buddy, 10s, "Hello World!")
  //   .then(
  //     // ... wait up to 10s for a response ...
  //     [self](const std::string& what) {
  //       // ... and print it
  //       aout(self).println("{}", what);
  //     });

  caf::json_writer writer;
  vector<int> d{1,2,3,4};
  struct result_j res{85, 95, "This is the score", d};
  //string payload;
  if(!writer.apply(res)){
      std::cerr << "failed to generate JSON output: "
                    << to_string(writer.get_error()) << '\n';
          throw std::logic_error("failed to generate JSON");
  }
  //writer.reset();
  string payload{writer.str()};
  struct user jane{1, "Jane Doe", "jane@doe.com", payload};
  auto msg=make_message(jane);
  self->send(buddy, msg);
}

void caf_main(actor_system& sys) {
  // create a new actor that calls 'mirror()'
  auto mirror_actor = sys.spawn(mirror);
  // create another actor that calls 'hello_world(mirror_actor)';
  sys.spawn(hello_world, mirror_actor);
  // the system will wait until both actors are done before exiting the program
}

// creates a main function for us that calls our caf_main
CAF_MAIN(caf::id_block::hello_world)
