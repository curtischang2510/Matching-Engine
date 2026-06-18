# Networking

- I want to simulate retrieving market data via over a network and feed this data into my order book. The thing is I don't really want to call an API because that might not be fast enough and I don't want to pay 

- I want to somehow simulate this idea of receiving market data but am not sure how to do it. Should I have another process running in the background that retrieves market data from somewhere (maybe from disk as a file) or should I let my order book read directly from the file. Im less inclined to use the former because typically data over the network gets injested immedately by the program and we don't read it from disk this could add a lot of unwanted latency
- This second program can be done in either Python or C++ whichever you think is more fitting
- i do want to get networking experience in C++ but in the C++ program in main i need to also set up a server portion to receive the data so that covers it already