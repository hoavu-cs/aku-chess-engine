import numpy as np
import chess



# Convert FEN to NNUE input format (12x64 one-hot encoding)
def fen_to_input(fen):
    board = chess.Board(fen)
    input_tensor = np.zeros((12, 64), dtype=np.float32)

    piece_map = board.piece_map()
    for square, piece in piece_map.items():
        piece_idx = (piece.color * 6) + (piece.piece_type - 1)  # Matches C++ Order (Black first, White second)
        input_tensor[piece_idx][square] = 1  # One-hot encode pieces

    # Debugging: Print feature vector in 12 × 64 format (Matches C++)
    # print("\nPython Feature Vector (12x64 format) AFTER FIXING COLOR ENCODING TO MATCH C++:")
    # for piece_idx in range(12):
    #     print(f"\nPiece Type {piece_idx} (Row {piece_idx + 1}/12):")
    #     for row in range(8):
    #         print(" ".join(str(int(input_tensor[piece_idx, row * 8 + col])) for col in range(8)))

    flattened_vector = input_tensor.flatten()

    # Print flattened vector
    #print("\nPython Feature Vector (Flattened 768 values):")
    #print(" ".join(str(int(value)) for value in flattened_vector))

    return flattened_vector  # Return flattened input

# Load .npy weight files
def load_vector(filename, size):
    data = np.load(filename).astype(np.float32)
    if data.shape[0] != size:
        raise ValueError(f"Error: Expected {size} elements in {filename}, but got {data.shape[0]}")
    return data

def load_matrix(filename, rows, cols):
    data = np.load(filename).astype(np.float32)

    if data.shape == (cols, rows):  # If transposed, fix it
        print(f"Warning: Transposing {filename} to match expected shape ({rows}, {cols})")
        data = data.T  # Transpose the matrix

    if data.shape != (rows, cols):
        raise ValueError(f"Error: Expected ({rows}, {cols}) elements in {filename}, but got {data.shape}")
    
    return data


# ReLU activation function
def relu(x):
    return np.maximum(0, x)  # Matches C++ relu()

# NNUE forward inference
def nnue_inference(input_vector):
    # Load weights
    W1 = load_matrix("fc1.weight.npy", 1024, 768)
    B1 = load_vector("fc1.bias.npy", 1024)

    W2 = load_matrix("fc2.weight.npy", 1, 1024)
    B2 = load_vector("fc2.bias.npy", 1)

    # Fix shape if incorrect
    if W1.shape == (768, 1024):  
        print("Warning: Transposing W1 to match expected shape.")
        W1 = W1.T  # Fix shape if needed

    # Ensure input shape is correct
    input_vector = input_vector.flatten()  # Ensure it's (768,)
    if input_vector.shape != (768,):
        raise ValueError(f"Error: Input vector must have shape (768,), but got {input_vector.shape}")

    print(f"Input Vector Shape: {input_vector.shape}")  # Should be (768,)
    print(f"W1 Shape: {W1.shape}")  # Should be (1024, 768)

    # Forward pass
    x1 = relu(np.dot(W1, input_vector) + B1)  # First layer
    output = np.dot(W2, x1) + B2  # Final output layer

    return output[0]


# Main function
if __name__ == "__main__":
    fen = input("Enter FEN string: ")

    # Convert FEN to NNUE input
    input_vector = fen_to_input(fen)

    # Run inference
    eval_score = nnue_inference(input_vector) * 10000
    print(f"NNUE Evaluation Score: {eval_score}")
