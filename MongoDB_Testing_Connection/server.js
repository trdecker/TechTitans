const express = require('express');
const MongoClient = require('mongodb').MongoClient;

const app = express();
const PORT = 3002; // You can choose any other port if you like

const MONGO_URL = 'mongodb+srv://IndyBrown:EsjIUrLo37JA0uSA@questgang.1znyjli.mongodb.net/'; // Adjust this if your MongoDB server is on another host or port
const DATABASE_NAME = 'QuestGang'; // Change to your database name
let db;

app.get('/data', async (req, res) => {
    try {
        const collection = db.collection('test/books'); // Change 'your_collection_name' to the collection you want to query
        const data = await collection.find({}).toArray();
        res.json(data);
    } catch (error) {
        res.status(500).json({ message: 'Internal Server Error' });
    }
});

MongoClient.connect(MONGO_URL, { useUnifiedTopology: true }, (err, client) => {
    if (err) {
        console.error('Failed to connect to MongoDB', err);
        process.exit(1);
    }
    db = client.db(DATABASE_NAME);
    app.listen(PORT, () => {
        console.log(`Server is running on http://localhost:${PORT}`);
    });
});
