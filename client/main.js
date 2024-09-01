import { html, render } from 'https://cdn.jsdelivr.net/gh/lit/dist@3/core/lit-core.min.js';
import * as d3 from "https://cdn.jsdelivr.net/npm/d3@7/+esm";

const ws = new WebSocket('ws://localhost:9002');
const worker = new Worker('web_worker.js');
let orderBookData = { bids: [], asks: [] };
const marketOrders = [];
let chartData = { bidData: [], askData: [] };

ws.onopen = () => {
    console.log('Connected to WebSocket server');
};

ws.onerror = (error) => {
    console.error('WebSocket Error:', error);
};

ws.onclose = (event) => {
    console.warn('WebSocket Closed:', event.code, event.reason);
};

ws.onmessage = (event) => {
    worker.postMessage(event.data);
};

worker.onmessage = (event) => {
    const data = event.data;
    if (data.bids && data.asks) {
        orderBookData = data;
        renderOrderBook();
        updateChartData();
        updateChart();
    } else if (data.type === 'timestamp') {
        updateTimestamp(data.value);
    }
};

function updateTimestamp(timestamp) {
    const timestampElement = document.getElementById('timestamp');
    if (timestampElement) {
        timestampElement.textContent = `Current Time: ${timestamp}`;
    } else {
        console.error('Timestamp element not found');
    }
}

function renderOrderBook() {
    const template = html`
        ${orderBookData.asks.slice(0, 20).map(ask => html`
            <div class="orderbook-row ask-row">
                <span></span>
                <span class="price">${ask.price.toFixed(2)}</span>
                <span class="ask-volume">${ask.volume.toFixed(0)}</span>
            </div>
        `)}
        <div class="orderbook-row spread-row">
            <span></span><span></span><span></span>
        </div>
        ${orderBookData.bids.slice(0, 20).map(bid => html`
            <div class="orderbook-row bid-row">
                <span class="bid-volume">${bid.volume.toFixed(0)}</span>
                <span class="price">${bid.price.toFixed(2)}</span>
                <span></span>
            </div>
        `)}
    `;
    render(template, document.getElementById('orderbook-body'));
}

function addMarketOrder(order) {
    marketOrders.unshift(order);
    if (marketOrders.length > 20) marketOrders.pop();
    renderMarketOrders();
}

function renderMarketOrders() {
    const template = html`
        ${marketOrders.map(order => html`
            <div class="market-order-row ${order.side === 'buy' ? 'buy-order' : 'sell-order'}">
                <span class="time">${formatUnixTime(order.time)}</span>
                <span class="price">${order.price.toFixed(2)}</span>
                <span class="size">${order.size}</span>
            </div>
        `)}
    `;
    render(template, document.getElementById('market-orders-body'));
}

function formatUnixTime(unixTime) {
    const date = new Date(unixTime);
    const hours = date.getHours().toString().padStart(2, '0');
    const minutes = date.getMinutes().toString().padStart(2, '0');
    const seconds = date.getSeconds().toString().padStart(2, '0');
    const milliseconds = date.getMilliseconds().toString().padStart(3, '0');
    return `${hours}:${minutes}:${seconds}.${milliseconds}`;
}

function updateChartData() {
    chartData.bidData = orderBookData.bids.slice(0, 20).map(d => ({price: d.price, volume: d.volume}));
    chartData.askData = orderBookData.asks.slice(0, 20).map(d => ({price: d.price, volume: d.volume}));
}

let chart, xScale, yScale, bidArea, askArea;

function initChart() {
    const margin = { top: 20, right: 20, bottom: 30, left: 50 };
    const width = document.querySelector('.chart-container').clientWidth - margin.left - margin.right;
    const height = document.querySelector('.chart-container').clientHeight - margin.top - margin.bottom;

    const svg = d3.select('#chart')
        .append('svg')
        .attr('width', width + margin.left + margin.right)
        .attr('height', height + margin.top + margin.bottom)
        .append('g')
        .attr('transform', `translate(${margin.left},${margin.top})`);

    xScale = d3.scaleLinear().range([0, width]);
    yScale = d3.scaleLinear().range([height, 0]);

    const areaGenerator = d3.area()
        .x(d => xScale(d.price))
        .y0(height)
        .y1(d => yScale(d.volume))
        .curve(d3.curveStepAfter);

    bidArea = svg.append("path")
        .attr("class", "area bid-area")
        .attr("fill", "url(#area-gradient-bid)");

    askArea = svg.append("path")
        .attr("class", "area ask-area")
        .attr("fill", "url(#area-gradient-ask)");

    svg.append("g")
        .attr("class", "x-axis")
        .attr("transform", `translate(0,${height})`);

    svg.append("g")
        .attr("class", "y-axis");

    // Add gradients
    const defs = svg.append("defs");

    const bidGradient = defs.append("linearGradient")
        .attr("id", "area-gradient-bid")
        .attr("gradientUnits", "userSpaceOnUse")
        .attr("x1", 0).attr("y1", height)
        .attr("x2", 0).attr("y2", 0);

    bidGradient.append("stop")
        .attr("offset", "0%")
        .attr("stop-color", "rgba(0, 116, 217, 0.1)");

    bidGradient.append("stop")
        .attr("offset", "100%")
        .attr("stop-color", "rgba(0, 116, 217, 0.5)");

    const askGradient = defs.append("linearGradient")
        .attr("id", "area-gradient-ask")
        .attr("gradientUnits", "userSpaceOnUse")
        .attr("x1", 0).attr("y1", height)
        .attr("x2", 0).attr("y2", 0);

    askGradient.append("stop")
        .attr("offset", "0%")
        .attr("stop-color", "rgba(255, 65, 54, 0.1)");

    askGradient.append("stop")
        .attr("offset", "100%")
        .attr("stop-color", "rgba(255, 65, 54, 0.5)");

    chart = { svg, width, height, areaGenerator };
}

function updateChart() {
    if (!chart) initChart();

    const allPrices = [...chartData.bidData, ...chartData.askData].map(d => d.price);
    const allVolumes = [...chartData.bidData, ...chartData.askData].map(d => d.volume);

    xScale.domain([d3.min(allPrices), d3.max(allPrices)]);
    yScale.domain([0, d3.max(allVolumes)]);

    chart.svg.select(".x-axis").call(d3.axisBottom(xScale).tickFormat(d => `$${d}`));
    chart.svg.select(".y-axis").call(d3.axisLeft(yScale));

    bidArea.datum(chartData.bidData).attr("d", chart.areaGenerator);
    askArea.datum(chartData.askData).attr("d", chart.areaGenerator);
}

initChart();

worker.onerror = (error) => {
    console.error('Web Worker Error:', error);
};

window.addEventListener('error', (event) => {
    console.error('Global error:', event.error);
});

