
#ifndef CTS_CHANNEL_HH
#define CTS_CHANNEL_HH

#include <cassert>
#include <atomic>
#include <memory>
#include <utility>

namespace cts {

    template <typename T, typename Channel> class ChannelTx;
    template <typename T, typename Channel> class ChannelRx;

    template <typename Channel>
    struct channel_traits {
        static constexpr bool is_single_producer = true;
        static constexpr bool is_single_consumer = true;

        struct Connection {
            Channel channel;
            std::atomic_size_t num_producers {0};
            std::atomic_size_t num_consumers {0};

            template <typename... Args>
            explicit Connection(Args&&... args)
                : channel {std::forward<Args>(args)...}
            {}
        };
    };

    template <typename T, typename Channel>
    [[nodiscard]] auto channel_endpoints(Channel&& channel) {
        using Connection = typename channel_traits<std::decay_t<Channel>>::Connection;
        auto const connection = std::make_shared<Connection>(std::forward<Channel>(channel));
        return std::tuple{
            ChannelTx<T,std::decay_t<Channel>>{connection}, ChannelRx<T,std::decay_t<Channel>>{connection}
        };
    }

    template <typename T, typename Channel>
    class ChannelTx {

        friend auto channel_endpoints<T,Channel>(Channel&&);
        friend auto channel_endpoints<T,Channel&>(Channel&);

        using Connection = typename channel_traits<Channel>::Connection;
        std::shared_ptr<Connection> connection_;

        explicit ChannelTx(std::shared_ptr<Connection> connection)
            : connection_{std::move(connection)}
        {
            assert(connection_ && "expected a channel connection");
            connection_->num_producers += 1;
        }

    public:

        ~ChannelTx() {
            release();
        }

        ChannelTx(ChannelTx const&)
            requires(channel_traits<Channel>::is_single_producer == false) = default;

        ChannelTx& operator=(ChannelTx const&)
            requires(channel_traits<Channel>::is_single_producer == false) = default;

        ChannelTx(ChannelTx&& other) noexcept
            : connection_{std::exchange(other.connection_, nullptr)}
        {}

        ChannelTx& operator=(ChannelTx&& other) noexcept {
            std::swap(*this, other); return *this;
        }

        friend void swap(ChannelTx& a, ChannelTx& b) noexcept {
            std::swap(a.connection_, b.connection_);
        }

        void release() noexcept {
            if (connection_) { connection_->num_producers -= 1; }
            connection_.reset();
        }

        [[nodiscard]] bool disconnected() const {
            return connection_->num_consumers == 0;
        }

        [[nodiscard]] auto send_available() const noexcept {
            return connection_->channel.send_available();
        }

        void send(T const& msg) { connection_->channel.send(msg); }
        void send(T&& msg) { connection_->channel.send(std::move(msg)); }

        template <typename... Args>
        void send_emplace(Args&&... args) {
            connection_->channel.send_emplace(std::forward<Args>(args)...);
        }

    };

    template <typename T, typename Channel>
    class ChannelRx {

        friend auto channel_endpoints<T,Channel>(Channel&&);
        friend auto channel_endpoints<T,Channel&>(Channel&);

        using Connection = typename channel_traits<Channel>::Connection;
        std::shared_ptr<Connection> connection_;

        explicit ChannelRx(std::shared_ptr<Connection> connection)
            : connection_{std::move(connection)}
        {
            assert(connection_ && "expected a channel connection");
            connection_->num_consumers += 1;
        }

    public:

        ~ChannelRx() {
            release();
        }

        ChannelRx(ChannelRx const&)
            requires(channel_traits<Channel>::is_single_consumer == false) = default;

        ChannelRx& operator=(ChannelRx const&)
            requires(channel_traits<Channel>::is_single_consumer == false) = default;

        ChannelRx(ChannelRx&& other) noexcept
            : connection_{std::exchange(other.connection_, nullptr)}
        {}

        ChannelRx& operator=(ChannelRx&& other) noexcept {
            std::swap(*this, other); return *this;
        }

        friend void swap(ChannelRx& a, ChannelRx& b) noexcept {
            std::swap(a.connection_, b.connection_);
        }

        void release() noexcept {
            if (connection_) { connection_->num_consumers -= 1; }
            connection_.reset();
        }

        [[nodiscard]] bool disconnected() const {
            return connection_->num_producers == 0;
        }

        [[nodiscard]] auto recv_available() const noexcept {
            return connection_->channel.recv_available();
        }

        [[nodiscard]] auto recv() { return connection_->channel.recv(); }

        void discard_next() { connection_->channel.discard_next(); }
        void discard_all() { connection_->channel.discard_all(); }

    };

} // namespace cts

#endif /* CTS_CHANNEL_HH */
