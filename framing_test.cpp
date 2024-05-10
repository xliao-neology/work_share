
#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/anon_mail.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include <caf/actor_ostream.hpp>
#include <caf/json_reader.hpp>
#include <caf/json_writer.hpp>

#include <iostream>

using namespace caf;
using namespace std;

struct TzcMessage {
  std::string source;       //!< name of the source actor
  std::string message_type; //!< type of message, such as heartbeat, etc.
  std::string payload;      //!< contains the message payload, which is expected
                            //!< to be a string in the json format
  TzcMessage(string Source = "", string Message_type = "",
             string Payload = "") noexcept
    : source(Source), message_type(Message_type), payload(Payload) {
  }
  TzcMessage(const TzcMessage&) = default;
  TzcMessage& operator=(const TzcMessage&) = default;

  std::string to_string() const {
    return "$:" + source + "$:" + message_type + "$:" + payload;
  }
};

struct payload_j {
  uint32_t a;
  uint32_t b;
  string c;
  vector<int> d;
};

CAF_BEGIN_TYPE_ID_BLOCK(framing_test, caf::first_custom_type_id)

  CAF_ADD_TYPE_ID(framing_test, (TzcMessage))
  CAF_ADD_TYPE_ID(framing_test, (payload_j))

CAF_END_TYPE_ID_BLOCK(framing_test)

template <class Inspector>
bool inspect(Inspector& f, TzcMessage& x) {
  return f.object(x).fields(f.field("source", x.source),
                            f.field("message_type", x.message_type),
                            f.field("payload", x.payload));
}

template <class Inspector>
bool inspect(Inspector& f, struct payload_j& x) {
  return f.object(x).fields(f.field("a", x.a), f.field("b", x.b),
                            f.field("c", x.c), f.field("d", x.d));
}

struct framing_actor_message_handler {
  explicit framing_actor_message_handler(caf::event_based_actor* selfptr,
                                         caf::actor p)
    : self(selfptr), parent(p) {
    self->set_down_handler([this](const caf::down_msg& dm) {
      if (dm.source == parent) {
        self->quit();
      }
    });
  }

  caf::behavior make_behavior() {
    return {[this](const std::string& message) {

            },
            [this](const TzcMessage& what) {
              struct payload_j payload;
              // aout(self).println(" \"id\": {}, \"name\": {}, \"email\": {}",
              // what.source, what.tp, what.message_type);
              aout(self).println(" \"source\": {}, \"message_type\": {}",
                                 what.source, what.message_type);
              caf::json_reader reader;

              if (!reader.load(what.payload)) {
                aout(self).println("error with loading json");
                aout(self) << "failed to parse JSON input: "
                           << to_string(reader.get_error()) << '\n';
                throw std::logic_error("failed to parse JSON");
              }
              if (reader.apply(payload)) {
                aout(self).println("parse json: {}, {}, {}", payload.a,
                                   payload.b, payload.c); //?
                for (auto& i : payload.d) {
                  aout(self).println("in vector:{}", i);
                }
              } else {
                aout(self).println("failed to parse json: {}",
                                   reader.get_error());
              }
              self->quit();
            },
            [this](caf::exit_msg& em) { self->quit(); }};
  };

  caf::event_based_actor* self;
  std::unordered_map<std::string, std::vector<std::string>> mp;
  caf::actor parent;
};

class framing_actor_impl : public caf::event_based_actor {
  // public:
  //     framing_actor_impl(caf::actor_config &cfg, std::string name );
  //     caf::behavior make_behavior() override;
  //     const char *name() const override;
private:
  // void on_exit();
  std::string name_;
  caf::expected<caf::actor> framing_message_handler_;

public:  
  framing_actor_impl(caf::actor_config& cfg, std::string name, caf::actor ptr)
    : caf::event_based_actor(cfg), name_(name), framing_message_handler_(ptr) {
    auto framing_message_handler = this->system().spawn(
      caf::actor_from_state<struct framing_actor_message_handler>, this);
    if (!framing_message_handler) {
      this->send_exit(this, caf::exit_reason::user_shutdown);
    } else {
      framing_message_handler_ = framing_message_handler;

      set_down_handler([this](const caf::down_msg& downMsg) {
        aout(this) << ("down message received from:"
                       + std::to_string(this->current_sender()->id()));
      });
    }
  }

  caf::behavior make_behavior() {
    return {[this](caf::exit_msg& em) { ; },
            [this](const TzcMessage& message) {
              // aout(this).println("receive message: {}", message.to_string());
              this->send(framing_message_handler_.value(), message);
            },
            [this](const std::string& message) {

            }};
  }

};

namespace {

WITH_FIXTURE(caf::test::fixture::deterministic) {

TEST("framing actor basic test") {
  auto framing_actor = sys.spawn<framing_actor_impl>( "framing_actor", nullptr);
   //auto framing_actor = sys.spawn( actor_from_state<framing_actor_impl>, "framing_actor", nullptr);

  caf::json_writer writer;
  vector<int> d{1, 2, 3, 4};
  struct payload_j res {
    85, 95, "score", d
  };
  if (!writer.apply(res)) {
    std::cout << "failed to generate JSON output: "
              << to_string(writer.get_error()) << '\n';
    throw std::logic_error("failed to generate JSON");
  }
  // writer.reset();
  string payload{writer.str()};

  struct TzcMessage mg {
    "self", "test", payload
  };

  //auto msg = make_message(mg);
  scoped_actor self{sys};
  self->send(framing_actor, mg);
  expect<TzcMessage>()
    .with(std::ref(mg))
    .from(self)
    .to(framing_actor);

  dispatch_message();
  
  check(self->mailbox().empty());  
}


} // WITH_FIXTURE(caf::test::fixture::deterministic)

TEST_INIT() {
  caf::init_global_meta_objects<caf::id_block::framing_test>();
}

} // namespace

CAF_TEST_MAIN()
