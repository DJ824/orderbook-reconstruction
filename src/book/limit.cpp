
#include "limit.h"

Limit::Limit(int32_t price) {
    volume_ = 0;
    num_orders_ = 0;
    price_ = price;
    head_ = nullptr;
    tail_ = nullptr;
}

Limit::Limit(Order* new_order) :
        price_(new_order->price_),
        volume_(new_order->size),
        num_orders_(1),
        head_(new_order),
        tail_(new_order) {};

void Limit::add_order(Order *new_order) {
    if (head_ == nullptr && tail_ == nullptr) {
        head_ = new_order;
        tail_ = new_order;
    } else {
        tail_->next_ = new_order;
        new_order->prev_ = tail_;
        tail_ = tail_->next_;
        tail_->next_ = nullptr;
    }
    volume_ += new_order->size;
    ++num_orders_;
    new_order->parent_ = this;
}

void Limit::remove_order(Order *target) {
    volume_ -= target->size;
    --num_orders_;

    if (head_ == tail_ && head_ == target) {
        head_ = nullptr;
        tail_ = nullptr;
    }
    else if (head_ == target) {
        head_ = head_->next_;
        if (head_) {
            head_->prev_ = nullptr;
        }
    }
    else if (tail_ == target) {
        tail_ = tail_->prev_;
        if (tail_) {
            tail_->next_ = nullptr;
        }
    }
    else {
        if (target->prev_) {
            target->prev_->next_ = target->next_;
        }
        if (target->next_) {
            target->next_->prev_ = target->prev_;
        }
    }

    target->next_ = nullptr;
    target->prev_ = nullptr;
    target->parent_ = nullptr;
}

void Limit::set(int32_t price) { this->price_ = price; }

void Limit::reset() {
    this->price_ = 0;
    this->volume_ = 0;
    this->num_orders_ = 0;
    this->head_ = nullptr;
    this->tail_ = nullptr;
}


bool Limit::is_empty() { return head_ == nullptr && tail_ == nullptr; }



