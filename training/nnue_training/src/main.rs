// use bullet_lib::trainer::save::QuantTarget;
// use bullet_lib::{
//     nn::{optimiser, Activation},
//     trainer::{
//         default::{
//             formats::sfbinpack::{
//                 chess::{piecetype::PieceType, r#move::MoveType},
//                 TrainingDataEntry,
//             },
//             inputs, loader, Loss, TrainerBuilder,
//         },
//         schedule::{lr, wdl, TrainingSchedule, TrainingSteps},
//         settings::LocalSettings,
//     },
// };

// // const HL1: usize = 16;
// // const HL2: usize = 16;
// // const QA: i16 = 255;
// // const QB: i16 = 64;
// const SCALE: i32 = 400;

// fn main() {
//     let mut trainer = TrainerBuilder::default()
//         .advanced_quantisations(&[QuantTarget::Float, QuantTarget::Float, QuantTarget::Float, QuantTarget::Float])
//         .optimiser(optimiser::AdamW)
//         .loss_fn(Loss::SigmoidMSE)
//         .input(inputs::Chess768)
//         .feature_transformer(768)
//         .activate(Activation::SCReLU)
//         .add_layer(8)
//         .activate(Activation::CReLU)
//         .add_layer(16)
//         .activate(Activation::CReLU)
//         .add_layer(1)
//         .build();


//     trainer.load_from_checkpoint("checkpoints_v2/deep_simple_256-30/");

//     let schedule = TrainingSchedule {
//         net_id: "deep_simple_256".to_string(),
//         eval_scale: SCALE as f32,
//         steps: TrainingSteps {
//             batch_size: 16_384,
//             batches_per_superbatch: 6104,
//             start_superbatch: 31,
//             end_superbatch: 200,
//         },
//         wdl_scheduler: wdl::ConstantWDL { value: 0.75 },
//         lr_scheduler: lr::StepLR { start: 0.001, gamma: 0.1, step: 50 },
//         save_rate: 5,
//     };

//     trainer.set_optimiser_params(optimiser::AdamWParams::default());

//     let settings = LocalSettings {
//         threads: 8,
//         test_set: None,
//         output_directory: "checkpoints_v2",
//         batch_queue_size: 64,
//     };

//     let data_loader = {
//         let file_path = "janfebmaraprmayjun.binpack";
//         let buffer_size_mb = 1024;
//         let threads = 8;
//         fn filter(entry: &TrainingDataEntry) -> bool {
//             entry.ply >= 16
//                 && !entry.pos.is_checked(entry.pos.side_to_move())
//                 && entry.score.unsigned_abs() <= 10000
//                 && entry.mv.mtype() == MoveType::Normal
//                 && entry.pos.piece_at(entry.mv.to()).piece_type() == PieceType::None
//         }

//         loader::SfBinpackLoader::new(file_path, buffer_size_mb, threads, filter)
//     };

//     trainer.run(&schedule, &settings, &data_loader);
// }


/*
This is about as simple as you can get with a network, the arch is
    (768 -> HIDDEN_SIZE)x2 -> 1
and the training schedule is pretty sensible.
There's potentially a lot of elo available by adjusting the wdl
and lr schedulers, depending on your dataset.
*/

use bullet_lib::{
    nn::{optimiser, Activation},
    trainer::{
        default::{
            formats::sfbinpack::{
                chess::{piecetype::PieceType, r#move::MoveType},
                TrainingDataEntry,
            },
            inputs, loader, Loss, TrainerBuilder,
        },
        schedule::{lr, wdl, TrainingSchedule, TrainingSteps},
        settings::LocalSettings,
    },
};

const HIDDEN_SIZE: usize = 1280;
const SCALE: i32 = 400;
const QA: i16 = 255;
const QB: i16 = 64;

fn main() {
    let mut trainer = TrainerBuilder::default()
        .quantisations(&[QA, QB])
        .optimiser(optimiser::AdamW)
        .loss_fn(Loss::SigmoidMSE)
        .input(inputs::Chess768)
        .feature_transformer(HIDDEN_SIZE)
        .activate(Activation::SCReLU)
        .add_layer(1)
        .build();
    
    //trainer.load_from_checkpoint("checkpoints_may/simple1024_v3-80/");

    let schedule = TrainingSchedule {
        net_id: "simple1280_v1".to_string(),
        eval_scale: SCALE as f32,
        steps: TrainingSteps {
            batch_size: 16_384,
            batches_per_superbatch: 6104,
            start_superbatch: 1,
            end_superbatch: 200,
        },
        wdl_scheduler: wdl::ConstantWDL { value: 0.05 },
        lr_scheduler: lr::StepLR { start: 0.001, gamma: 0.2, step: 30 },
        save_rate: 5,
    };

    trainer.set_optimiser_params(optimiser::AdamWParams::default());

    let settings = LocalSettings { threads: 8, test_set: None, output_directory: "checkpoints_may", batch_queue_size: 64 };

    // loading from a SF binpack
    let data_loader = {
        let file_path = "big.binpack";
        let buffer_size_mb = 1024;
        let threads = 4;
        fn filter(entry: &TrainingDataEntry) -> bool {
            entry.ply >= 16
                && !entry.pos.is_checked(entry.pos.side_to_move())
                && entry.score.unsigned_abs() <= 10000
                && entry.mv.mtype() == MoveType::Normal
                && entry.pos.piece_at(entry.mv.to()).piece_type() == PieceType::None
        }

        loader::SfBinpackLoader::new(file_path, buffer_size_mb, threads, filter)
    };

    trainer.run(&schedule, &settings, &data_loader);

}

/*
This is how you would load the network in rust.
Commented out because it will error if it can't find the file.
static NNUE: Network =
    unsafe { std::mem::transmute(*include_bytes!("../checkpoints/simple-10/simple-10.bin")) };
*/

#[inline]
/// Squared Clipped ReLU - Activation Function.
/// Takes i16 input and returns i32 output.
fn screlu(x: i16) -> i32 {
    let clamped = i32::from(x).clamp(0, i32::from(QA));
    clamped * clamped
}


/// This is the quantised format that bullet outputs.
#[repr(C)]
pub struct Network {
    /// Column-Major `HIDDEN_SIZE x 768` matrix.
    feature_weights: [Accumulator; 768],
    /// Vector with dimension `HIDDEN_SIZE`.
    feature_bias: Accumulator,
    /// Column-Major `1 x (2 * HIDDEN_SIZE)`
    /// matrix, we use it like this to make the
    /// code nicer in `Network::evaluate`.
    output_weights: [i16; 2 * HIDDEN_SIZE],
    /// Scalar output bias.
    output_bias: i16,
}

impl Network {
    /// Calculates the output of the network, starting from the already
    /// calculated hidden layer (done efficiently during makemoves).
    pub fn evaluate(&self, us: &Accumulator, them: &Accumulator) -> i32 {
        // Initialise output with bias.
        let mut output = 0;
        

        // Side-To-Move Accumulator -> Output.
        for (&input, &weight) in us.vals.iter().zip(&self.output_weights[..HIDDEN_SIZE]) {
            output += screlu(input) * i32::from(weight);
        }

        // Not-Side-To-Move Accumulator -> Output.
        for (&input, &weight) in them.vals.iter().zip(&self.output_weights[HIDDEN_SIZE..]) {
            output += screlu(input) * i32::from(weight);
        }

        // Normalize output and add bias
        let raw = SCALE * (output / i32::from(QA) + i32::from(self.output_bias));
        let final_eval = raw / (i32::from(QA) * i32::from(QB));

        final_eval
    }
}

/// A column of the feature-weights matrix.
/// Note the `align(64)`.
#[derive(Clone, Copy)]
#[repr(C, align(64))]
pub struct Accumulator {
    vals: [i16; HIDDEN_SIZE],
}

impl Accumulator {
    /// Initialised with bias so we can just efficiently
    /// operate on it afterwards.
    pub fn new(net: &Network) -> Self {
        net.feature_bias
    }

    /// Add a feature to an accumulator.
    pub fn add_feature(&mut self, feature_idx: usize, net: &Network) {
        for (i, d) in self.vals.iter_mut().zip(&net.feature_weights[feature_idx].vals) {
            *i += *d
        }
    }

    /// Remove a feature from an accumulator.
    pub fn remove_feature(&mut self, feature_idx: usize, net: &Network) {
        for (i, d) in self.vals.iter_mut().zip(&net.feature_weights[feature_idx].vals) {
            *i -= *d
        }
    }
}