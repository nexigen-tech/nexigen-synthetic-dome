//! Validated intermediate model for contract schemas and component descriptors.
//!
//! The model is intentionally plain: it contains only resolved, validated
//! information needed for canonicalization and code generation.  Cold-path
//! metadata such as descriptions and units are preserved; source formatting
//! and paths are not.

/// Simple three-part version used for contracts, components, and SDK ABI.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Version {
    pub major: u64,
    pub minor: u64,
    pub patch: u64,
}

impl Version {
    pub fn new(major: u64, minor: u64, patch: u64) -> Self {
        Self {
            major,
            minor,
            patch,
        }
    }
}

impl std::fmt::Display for Version {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
    }
}

/// Fixed-width scalar types allowed on the wire.
/// `bool`, `long`, `size_t`, pointers, and data-carrying enums are excluded
/// by design so C++ and Rust produce identical, portable layouts.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PrimitiveType {
    I8,
    I16,
    I32,
    I64,
    U8,
    U16,
    U32,
    U64,
    F32,
    F64,
}

impl PrimitiveType {
    /// Returns true if the primitive is an integer type (signed or unsigned).
    pub fn is_integer(&self) -> bool {
        matches!(
            self,
            PrimitiveType::I8
                | PrimitiveType::I16
                | PrimitiveType::I32
                | PrimitiveType::I64
                | PrimitiveType::U8
                | PrimitiveType::U16
                | PrimitiveType::U32
                | PrimitiveType::U64
        )
    }
}

/// Array count: either a literal or a template parameter reference.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum ArrayCount {
    Literal(usize),
    Param(String),
}

/// A type expression after parsing and name resolution.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Type {
    /// Fixed-width scalar.
    Primitive(PrimitiveType),
    /// Reference to a named record or enum declared in the schema.
    Named(String),
    /// Fixed-size array: element type and element count.
    Array(Box<Type>, ArrayCount),
}

impl Type {
    /// Returns the element type if this is an array.
    pub fn array_element(&self) -> Option<&Type> {
        match self {
            Type::Array(ty, _) => Some(ty),
            _ => None,
        }
    }

    /// Returns the element count if this is a literal array.
    pub fn array_count(&self) -> Option<usize> {
        match self {
            Type::Array(_, ArrayCount::Literal(n)) => Some(*n),
            _ => None,
        }
    }

    /// Returns true if this array's size depends on a template parameter.
    pub fn is_parametric_array(&self) -> bool {
        matches!(self, Type::Array(_, ArrayCount::Param(_)))
    }
}

/// One enum constant.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct EnumValue {
    pub name: String,
    pub value: u64,
}

/// Named enum definition with a fixed underlying integer type.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct EnumDef {
    pub name: String,
    pub underlying: PrimitiveType,
    pub values: Vec<EnumValue>,
}

/// One field inside a record or the top-level aggregate.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Field {
    pub name: String,
    pub ty: Type,
    pub description: Option<String>,
    pub unit: Option<String>,
    /// Optional inclusive minimum for scalar numeric fields.
    pub min: Option<i64>,
    /// Optional inclusive maximum for scalar numeric fields.
    pub max: Option<i64>,
    /// Optional default value kept as a source string for cold-path tooling.
    pub default: Option<String>,
}

/// Named record definition.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct RecordDef {
    pub name: String,
    pub fields: Vec<Field>,
}

/// A user-defined type inside a schema.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum TypeDef {
    Record(RecordDef),
    Enum(EnumDef),
}

impl TypeDef {
    pub fn name(&self) -> &str {
        match self {
            TypeDef::Record(r) => &r.name,
            TypeDef::Enum(e) => &e.name,
        }
    }
}

/// Template parameter declaration.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct TemplateParam {
    pub name: String,
    pub kind: TemplateParamKind,
}

/// Kind of template parameter.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum TemplateParamKind {
    /// Integer parameter: type, optional range, optional default.
    Integer {
        ty: PrimitiveType,
        min: Option<i64>,
        max: Option<i64>,
        default: Option<i64>,
    },
    /// Type parameter: selected from an explicit allow-list of primitives.
    TypeParam {
        allowed: Vec<PrimitiveType>,
        default: Option<PrimitiveType>,
    },
}

/// Concrete instantiation of a template contract.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ConcreteInstance {
    pub name: String,
    pub values: ConcreteValues,
}

/// Concrete values for the two kinds of template parameters.
#[derive(Debug, Clone, PartialEq, Eq, Hash, Default)]
pub struct ConcreteValues {
    pub integers: Vec<(String, i64)>,
    pub types: Vec<(String, PrimitiveType)>,
}

/// Parsed and validated contract schema.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ContractSchema {
    pub name: String,
    pub version: Version,
    pub description: Option<String>,
    pub params: Vec<TemplateParam>,
    pub concrete: Vec<ConcreteInstance>,
    pub types: Vec<TypeDef>,
    pub fields: Vec<Field>,
}

/// Reference to a required contract from a component descriptor.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ContractRef {
    pub name: String,
    pub version: Version,
    pub layout_hash: Option<String>,
}

/// Direction of a logical port.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PortDirection {
    In,
    Out,
    InOut,
}

/// Logical semantics of a port.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum PortSemantics {
    Signal,
    Event,
    Stream,
    Command,
}

/// One logical port declaration.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Port {
    pub name: String,
    pub direction: PortDirection,
    pub semantics: PortSemantics,
    pub contract_name: String,
    pub contract_version: Version,
    pub layout_hash: Option<String>,
    pub binding_required: bool,
}

/// SDK ABI version required by a component.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct SdkVersion {
    pub abi_major: u64,
    pub abi_minor: u64,
}

/// Typed parameter declaration.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Parameter {
    pub name: String,
    pub ty: Type,
    pub description: Option<String>,
    pub default: Option<String>,
    pub min: Option<i64>,
    pub max: Option<i64>,
    pub file: bool,
    pub required: bool,
}

/// Thread role declared by a component.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ThreadRole {
    pub name: String,
    pub required: bool,
    pub single_cpu: bool,
    pub default_schedule: Option<String>,
}

/// Resource and determinism declarations.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Resources {
    pub max_heap_bytes: Option<u64>,
    pub deterministic: bool,
    pub uses_rng: bool,
    pub requires_hardware_rt: bool,
}

impl Default for Resources {
    fn default() -> Self {
        Self {
            max_heap_bytes: None,
            deterministic: true,
            uses_rng: false,
            requires_hardware_rt: false,
        }
    }
}

/// Parsed and validated component descriptor.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ComponentDescriptor {
    pub name: String,
    pub version: Version,
    pub sdk: SdkVersion,
    pub contracts: Vec<ContractRef>,
    pub ports: Vec<Port>,
    pub parameters: Vec<Parameter>,
    pub thread_roles: Vec<ThreadRole>,
    pub resources: Resources,
    pub supported_modes: Vec<String>,
}
