const express = require('express');
const MongoClient = require('mongodb').MongoClient;

const app = express();
const PORT = 3000; // You can choose any other port if you like

const MONGO_URL = 'mongodb://localhost:27017'; // Adjust this if your MongoDB server is on another host or port
const DATABASE_NAME = 'sample'; // Change to your database name
let db;

app.get('/data', async (req, res) => {
    try {
        const collection = db.collection('your_collection_name'); // Change 'your_collection_name' to the collection you want to query
        const data = await collection.find({}).toArray();
        res.json(data);
    } catch (error) {
        res.status(500).json({ message: 'Internal Server Error' });
    }
});

// Endpoint to add a new book
app.post('/books', async (req, res) => {
    try {
        const newBook = new Book(req.body);
        const savedBook = await newBook.save();
        res.json(savedBook);
    } catch (err) {
        res.status(500).send("Error saving book");
    }
});
