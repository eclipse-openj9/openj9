package v1beta1

import (
	certmngrv1alpha2 "github.com/jetstack/cert-manager/pkg/apis/certmanager/v1alpha2"
	cmmeta "github.com/jetstack/cert-manager/pkg/apis/meta/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// Certificate is used to request certificates
type Certificate struct {
	// CommonName is a common name to be used on the Certificate.
	// The CommonName should have a length of 64 characters or fewer to avoid
	// generating invalid CSRs.
	// +optional
	CommonName string `json:"commonName,omitempty"`

	// Organization is the organization to be used on the Certificate
	// +optional
	Organization []string `json:"organization,omitempty"`

	// Certificate default Duration
	// +optional
	Duration *metav1.Duration `json:"duration,omitempty"`

	// Certificate renew before expiration duration
	// +optional
	RenewBefore *metav1.Duration `json:"renewBefore,omitempty"`

	// DNSNames is a list of subject alt names to be used on the Certificate.
	// +optional
	DNSNames []string `json:"dnsNames,omitempty"`

	// IPAddresses is a list of IP addresses to be used on the Certificate
	// +optional
	IPAddresses []string `json:"ipAddresses,omitempty"`

	// URISANs is a list of URI Subject Alternative Names to be set on this
	// Certificate.
	// +optional
	URISANs []string `json:"uriSANs,omitempty"`

	// SecretName is the name of the secret resource to store this secret in
	SecretName string `json:"secretName,omitempty"`

	// IssuerRef is a reference to the issuer for this certificate.
	// If the 'kind' field is not set, or set to 'Issuer', an Issuer resource
	// with the given name in the same namespace as the Certificate will be used.
	// If the 'kind' field is set to 'ClusterIssuer', a ClusterIssuer with the
	// provided name will be used.
	// The 'name' field in this stanza is required at all times.
	IssuerRef cmmeta.ObjectReference `json:"issuerRef,omitempty"`

	// IsCA will mark this Certificate as valid for signing.
	// This implies that the 'cert sign' usage is set
	// +optional
	IsCA bool `json:"isCA,omitempty"`

	// Usages is the set of x509 actions that are enabled for a given key. Defaults are ('digital signature', 'key encipherment') if empty
	// +optional
	Usages []certmngrv1alpha2.KeyUsage `json:"usages,omitempty"`

	// KeySize is the key bit size of the corresponding private key for this certificate.
	// If provided, value must be between 2048 and 8192 inclusive when KeyAlgorithm is
	// empty or is set to "rsa", and value must be one of (256, 384, 521) when
	// KeyAlgorithm is set to "ecdsa".
	// +optional
	KeySize int `json:"keySize,omitempty"`

	// KeyAlgorithm is the private key algorithm of the corresponding private key
	// for this certificate. If provided, allowed values are either "rsa" or "ecdsa"
	// If KeyAlgorithm is specified and KeySize is not provided,
	// key size of 256 will be used for "ecdsa" key algorithm and
	// key size of 2048 will be used for "rsa" key algorithm.
	// +optional
	KeyAlgorithm certmngrv1alpha2.KeyAlgorithm `json:"keyAlgorithm,omitempty"`

	// KeyEncoding is the private key cryptography standards (PKCS)
	// for this certificate's private key to be encoded in. If provided, allowed
	// values are "pkcs1" and "pkcs8" standing for PKCS#1 and PKCS#8, respectively.
	// If KeyEncoding is not specified, then PKCS#1 will be used by default.
	KeyEncoding certmngrv1alpha2.KeyEncoding `json:"keyEncoding,omitempty"`
}

// GetSpec returns certificate spec
func (crt *Certificate) GetSpec() certmngrv1alpha2.CertificateSpec {
	newCrt := certmngrv1alpha2.CertificateSpec{}
	newCrt.CommonName = crt.CommonName
	newCrt.Organization = crt.Organization
	newCrt.Duration = crt.Duration
	newCrt.RenewBefore = crt.RenewBefore
	newCrt.DNSNames = crt.DNSNames
	newCrt.IPAddresses = crt.IPAddresses
	newCrt.URISANs = crt.URISANs
	newCrt.SecretName = crt.SecretName
	newCrt.IssuerRef = crt.IssuerRef
	newCrt.IsCA = crt.IsCA
	newCrt.Usages = crt.Usages
	newCrt.KeySize = crt.KeySize
	newCrt.KeyAlgorithm = crt.KeyAlgorithm
	newCrt.KeyEncoding = crt.KeyEncoding

	return newCrt
}
