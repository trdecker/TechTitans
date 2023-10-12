const express = require('express');
const mongoose = require('mongoose');
const bodyParser = require('body-parser');

const app = express();

// Replace this with your MongoDB connection string
const mongoURI = "mongodb+srv://IndyBrown:EsjIUrLo37JA0uSA@questgang.1znyjli.mongodb.net/";

mongoose.connect(mongoURI, {
    useNewUrlParser: true,
    useUnifiedTopology: true
}).then(() => {
    console.log("Connected to MongoDB");
}).catch(err => {
    console.error("Error connecting to MongoDB", err);
});

app.use(bodyParser.json());

const PORT = 3000;

app.listen(PORT, () => {
    console.log(`Server is running on port ${PORT}`);
});


const bookSchema = new mongoose.Schema({
    title: String,
    author: String,
    publishedYear: Number
});

const Book = mongoose.model('Book', bookSchema);

// Endpoint to get all books
app.get('/books', async (req, res) => {
    try {
        const books = await Book.find();
        res.json(books);
    } catch (err) {
        res.status(500).send("Error fetching books");
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
