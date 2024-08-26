
onmessage = function(event) {
    const data = JSON.parse(event.data);

    if (data.bids && data.asks) {
        const orderBookData = {
            bids: data.bids.map(bid => ({ price: parseFloat(bid.price), volume: parseFloat(bid.volume) })),
            asks: data.asks.map(ask => ({ price: parseFloat(ask.price), volume: parseFloat(ask.volume) }))
        };
        postMessage(orderBookData);
    }
    else if (data.time !== undefined && data.price !== undefined && data.size !== undefined && data.side !== undefined) {
        const marketOrder = {
            time: parseInt(data.time),
            price: parseFloat(data.price),
            size: parseInt(data.size),
            side: data.side
        };
        postMessage(marketOrder);
    }
};
